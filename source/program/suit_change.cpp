#include "suit_change.hpp"

#include <memory>
#include "lib.hpp"
#include "dread_types.hpp"
#include "game_functions.hpp"
#include "offsets.hpp"

namespace odr::suit_change {

	using game::Resolve;

	struct Offsets {
		/** Changes the model of a model updater component (sModelAlias setter). bool(updater, modelEntry). */
		ptrdiff_t ChangeModel;
		/** Adds a model to a material effect. void(effect, renderModel, matchMaterials). */
		ptrdiff_t AttachMaterialFx;
		/** Removes a model's current model data from a material effect. void(effect, renderModel). */
		ptrdiff_t DetachMaterialFx;
	};

	static constexpr offsets::Versioned<Offsets> OFFSETS = {
		.v1_0_0 = {
			.ChangeModel = 0x93677c,
			.AttachMaterialFx = 0x1455cc,
			.DetachMaterialFx = 0x1455dc,
		},
		.v2_1_0 = {
			.ChangeModel = 0x938fcc,
			.AttachMaterialFx = 0x1457fc,
			.DetachMaterialFx = 0x14580c,
		},
	};

	// Game types. Only used through pointers into game memory, and only the fields used here.

	/** Model data: its parts, which the effect draws. */
	struct CModelData {
		u8 unk0[0x34];
		u32 partCount;
		void* parts;
	};
	static_assert(offsetof(CModelData, partCount) == 0x34);
	static_assert(offsetof(CModelData, parts) == 0x38);

	struct CModelNode {
		u8 unk0[0x78];
		CModelData* modelData;
	};
	static_assert(offsetof(CModelNode, modelData) == 0x78);

	struct CRenderModel {
		u8 unk0[0x128];
		CModelNode* node;
	};
	static_assert(offsetof(CRenderModel, node) == 0x128);

	/** One model a material effect draws on. */
	struct SMaterialFxModel {
		CModelData* modelData;
		u8 unk8[0x8];
	};
	static_assert(sizeof(SMaterialFxModel) == 0x10);

	/** Render side of a material effect: the models it draws on. */
	struct CMaterialFxRender {
		u8 unk0[0x90];
		SMaterialFxModel* models;
		u32 modelCount;
	};
	static_assert(offsetof(CMaterialFxRender, models) == 0x90);
	static_assert(offsetof(CMaterialFxRender, modelCount) == 0x98);

	struct CEffect {
		u8 unk0[0xb];
		u8 type;
		u8 unkC[0x11c];
		CMaterialFxRender* render;
	};
	static_assert(offsetof(CEffect, type) == 0xb);
	static_assert(offsetof(CEffect, render) == 0x128);

	static const u8 EFFECT_TYPE_MATERIAL = 4;

	/** One effect an FX component plays. */
	struct SFxEntry {
		CEffect* effect;
		u8 unk8[0x18];
	};
	static_assert(sizeof(SFxEntry) == 0x20);

	/** An actor's FX component: the effects it plays. */
	struct CFxComponent {
		u8 unk0[0x50];
		SFxEntry* effects;
		u8 unk58[0x8];
		u32 effectCount;
	};
	static_assert(offsetof(CFxComponent, effects) == 0x50);
	static_assert(offsetof(CFxComponent, effectCount) == 0x60);

	struct CActor {
		u8 unk0[0x280];
		CFxComponent* fxComponent;
	};
	static_assert(offsetof(CActor, fxComponent) == 0x280);

	struct CModelUpdaterComponent {
		u8 unk0[0x30];
		/** The actor that owns the component. */
		CActor* actor;
		u8 unk38[0x18];
		/** Stays the same object when the model changes; the model data inside it is replaced. */
		CRenderModel* renderModel;
	};
	static_assert(offsetof(CModelUpdaterComponent, actor) == 0x30);
	static_assert(offsetof(CModelUpdaterComponent, renderModel) == 0x50);

	/**
	 * Adds a model to a material effect, so the effect draws on it. void(effect, renderModel, matchMaterials).
	 * matchMaterials: 1 = only the materials named in the effect (or the whole model if it names none),
	 * 0 = the whole model. The game passes 1 when it starts an effect.
	 */
	using AttachMaterialFxFn = void (*)(CEffect* effect, CRenderModel* renderModel, u32 matchMaterials);
	/** Removes a model's current model data from a material effect. void(effect, renderModel). */
	using DetachMaterialFxFn = void (*)(CEffect* effect, CRenderModel* renderModel);

	static AttachMaterialFxFn attachMaterialFx = nullptr;
	static DetachMaterialFxFn detachMaterialFx = nullptr;

	static CModelData* GetModelData(CRenderModel* renderModel) {
		if (renderModel == nullptr || renderModel->node == nullptr) {
			return nullptr;
		}
		return renderModel->node->modelData;
	}

	static bool EffectUsesModelData(CEffect* effect, CModelData* modelData) {
		if (effect->type != EFFECT_TYPE_MATERIAL) {
			return false;
		}
		CMaterialFxRender* render = effect->render;
		if (render == nullptr) {
			return false;
		}
		for (u32 i = 0; i < render->modelCount; i++) {
			if (render->models[i].modelData == modelData) {
				return true;
			}
		}
		return false;
	}

	/** The draw code reads every part the count says exists, so the parts must be there. */
	static bool ModelDataIsSafeToDraw(CModelData* modelData) {
		return modelData->partCount == 0 || modelData->parts != nullptr;
	}

	/**
	 * Changes the model of a model updater component. bool(modelUpdater, modelEntry).
	 * The game rebuilds the model but doesn't tell the material effects drawing on it,
	 * so they keep the old model and crash the renderer. Take them off the old model first and put them
	 * back on the new one afterwards.
	 */
	HOOK_DEFINE_TRAMPOLINE(ChangeModel) {
		static u64 Callback(CModelUpdaterComponent* modelUpdater, void* modelEntry) {
			// The actor this model updater belongs to (Samus) and the actor's FX component, which keeps every effect it plays
			CActor* actor = modelUpdater->actor;
			CFxComponent* fxComponent = actor != nullptr ? actor->fxComponent : nullptr;
			// The render model stays the same object; the model data inside it is what effects draw on
			CRenderModel* renderModel = modelUpdater->renderModel;
			CModelData* oldModelData = GetModelData(renderModel);
			if (fxComponent == nullptr || oldModelData == nullptr) {
				// No effects or no model yet, so nothing can point at the old model
				return Orig(modelUpdater, modelEntry);
			}

			// Take every material effect that draws on the old model data off the model.
			// Remember them, so they can be put back after the change.
			auto detached = std::make_unique<CEffect*[]>(fxComponent->effectCount);
			u32 detachedCount = 0;
			for (u32 i = 0; i < fxComponent->effectCount; i++) {
				CEffect* effect = fxComponent->effects[i].effect;
				if (effect == nullptr || !EffectUsesModelData(effect, oldModelData)) {
					continue;
				}
				detachMaterialFx(effect, renderModel);
				detached[detachedCount++] = effect;
			}

			// The original function changes the model
			u64 result = Orig(modelUpdater, modelEntry);

			// Put the effects back on the new model data, like the game does when it starts an effect
			renderModel = modelUpdater->renderModel; // defensive re-read
			CModelData* newModelData = GetModelData(renderModel);
			if (newModelData == nullptr || !ModelDataIsSafeToDraw(newModelData)) {
				// The new model isn't ready to be drawn. Should never happen (safety guard)
				return result;
			}
			for (u32 i = 0; i < detachedCount; i++) {
				attachMaterialFx(detached[i], renderModel, 1);
			}
			return result;
		}
	};

};

void odr::suit_change::InstallHooks() {
	const Offsets& offsets = OFFSETS.Get();
	if (!offsets::HasOffsets(offsets)) {
		return;
	}

	attachMaterialFx = Resolve<AttachMaterialFxFn>(offsets.AttachMaterialFx);
	detachMaterialFx = Resolve<DetachMaterialFxFn>(offsets.DetachMaterialFx);
	ChangeModel::InstallAtOffset(offsets.ChangeModel);
}
