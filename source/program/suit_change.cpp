#include "suit_change.hpp"

#include "game_functions.hpp"

namespace odr::suit_change {

	using game::Resolve;
	using game::Field;

	/**
	 * Adds a model to a material effect, so the effect draws on it. void(effect, renderModel, matchMaterials).
	 * matchMaterials: 1 = only the materials named in the effect (or the whole model if it names none),
	 * 0 = the whole model. The game passes 1 when it starts an effect.
	 */
	typedef void (*AttachMaterialFxFn)(void* effect, void* renderModel, u32 matchMaterials);
	/** Removes a model's current model data from a material effect. void(effect, renderModel). */
	typedef void (*DetachMaterialFxFn)(void* effect, void* renderModel);

	static AttachMaterialFxFn attachMaterialFx = NULL;
	static DetachMaterialFxFn detachMaterialFx = NULL;

	// Model updater component
	static const ptrdiff_t UPDATER_ACTOR = 0x30;
	static const ptrdiff_t UPDATER_RENDER_MODEL = 0x50;
	// Actor
	static const ptrdiff_t ACTOR_FX_COMPONENT = 0x280;
	// FX component: effects it plays, 0x20 bytes each, effect pointer first
	static const ptrdiff_t FX_EFFECTS = 0x50;
	static const ptrdiff_t FX_EFFECT_COUNT = 0x60;
	static const ptrdiff_t FX_EFFECT_STRIDE = 0x20;
	// Effect
	static const ptrdiff_t EFFECT_TYPE = 0xb;
	static const u8 EFFECT_TYPE_MATERIAL = 4;
	static const ptrdiff_t EFFECT_RENDER = 0x128;
	// Render side of a material effect: models it draws on, 0x10 bytes each, model data first
	static const ptrdiff_t EFFECT_RENDER_MODELS = 0x90;
	static const ptrdiff_t EFFECT_RENDER_MODEL_COUNT = 0x98;
	static const ptrdiff_t EFFECT_RENDER_MODEL_STRIDE = 0x10;
	// Render model -> model data
	static const ptrdiff_t RENDER_MODEL_NODE = 0x128;
	static const ptrdiff_t NODE_MODEL_DATA = 0x78;
	// Model data: its parts, which the effect draws
	static const ptrdiff_t MODEL_DATA_PART_COUNT = 0x34;
	static const ptrdiff_t MODEL_DATA_PARTS = 0x38;

	static const u32 MAX_EFFECTS = 64;

	static void* GetModelData(void* renderModel) {
		if (renderModel == NULL) {
			return NULL;
		}
		void* node = Field<void*>(renderModel, RENDER_MODEL_NODE);
		if (node == NULL) {
			return NULL;
		}
		return Field<void*>(node, NODE_MODEL_DATA);
	}

	static bool EffectUsesModelData(void* effect, void* modelData) {
		if (Field<u8>(effect, EFFECT_TYPE) != EFFECT_TYPE_MATERIAL) {
			return false;
		}
		void* render = Field<void*>(effect, EFFECT_RENDER);
		if (render == NULL) {
			return false;
		}
		char* models = Field<char*>(render, EFFECT_RENDER_MODELS);
		u32 count = Field<u32>(render, EFFECT_RENDER_MODEL_COUNT);
		for (u32 i = 0; i < count; i++) {
			if (Field<void*>(models, i * EFFECT_RENDER_MODEL_STRIDE) == modelData) {
				return true;
			}
		}
		return false;
	}

	/** The draw code reads every part the count says exists, so the parts must be there. */
	static bool ModelDataCanBeDrawn(void* modelData) {
		return Field<u32>(modelData, MODEL_DATA_PART_COUNT) == 0 || Field<void*>(modelData, MODEL_DATA_PARTS) != NULL;
	}

	/**
	 * Changes the model of a model updater component. bool(modelUpdater, modelEntry).
	 * The game rebuilds the model but doesn't tell the material effects drawing on it,
	 * so they keep the old model and crash the renderer. Take them off the old model first and put them
	 * back on the new one afterwards.
	 */
	HOOK_DEFINE_TRAMPOLINE(ChangeModel) {
		static u64 Callback(void* modelUpdater, void* modelEntry) {
			// The actor this model updater belongs to (Samus) and the actor's FX component, which keeps every effect it plays
			void* actor = Field<void*>(modelUpdater, UPDATER_ACTOR);
			void* fxComponent = actor != NULL ? Field<void*>(actor, ACTOR_FX_COMPONENT) : NULL;
			// The render model stays the same object; the model data inside it is what effects draw on
			void* renderModel = Field<void*>(modelUpdater, UPDATER_RENDER_MODEL);
			void* oldModelData = GetModelData(renderModel);
			if (fxComponent == NULL || oldModelData == NULL) {
				// No effects or no model yet, so nothing can point at the old model
				return Orig(modelUpdater, modelEntry);
			}

			// Take every material effect that draws on the old model data off the model.
			// Remember them, so they can be put back after the change.
			void* detached[MAX_EFFECTS];
			u32 detachedCount = 0;
			char* effects = Field<char*>(fxComponent, FX_EFFECTS);
			u32 effectCount = Field<u32>(fxComponent, FX_EFFECT_COUNT);
			for (u32 i = 0; i < effectCount; i++) {
				void* effect = Field<void*>(effects, i * FX_EFFECT_STRIDE);
				if (effect == NULL || !EffectUsesModelData(effect, oldModelData)) {
					continue;
				}
				detachMaterialFx(effect, renderModel);
				if (detachedCount < MAX_EFFECTS) {
					detached[detachedCount++] = effect;
				}
			}

			// The original function changes the model
			u64 result = Orig(modelUpdater, modelEntry);

			// Put the effects back on the new model data, like the game does when it starts an effect
			renderModel = Field<void*>(modelUpdater, UPDATER_RENDER_MODEL); // defensive re-read
			void* newModelData = GetModelData(renderModel);
			if (newModelData == NULL || !ModelDataCanBeDrawn(newModelData)) {
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

void odr::suit_change::InstallHooks(const functionOffsets* offsets) {
	if (offsets->ChangeModel == 0 || offsets->AttachMaterialFx == 0 || offsets->DetachMaterialFx == 0) {
		return;
	}
	attachMaterialFx = Resolve<AttachMaterialFxFn>(offsets->AttachMaterialFx);
	detachMaterialFx = Resolve<DetachMaterialFxFn>(offsets->DetachMaterialFx);
	ChangeModel::InstallAtOffset(offsets->ChangeModel);
}
