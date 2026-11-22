# BBSB authoring conventions

- Provide a reusable, directly placeable Blueprint for authored gameplay objects. Use `/Game/BBSB/Placeables` for shared presets and child Blueprints for variants.
- Use the shared interaction capabilities in `/Game/BBSB/Placeables/Interactions`. Configure movement, prompts, clips and condition targets on the placed instance. Reserve child Blueprints for a reusable change in geometry or behavior.
- Keep shared mechanics in native base classes. Expose instance/default settings, meshes, materials, class choices and explicit actor links for authoring. Do not require editing C++ to relocate a ladder, door, prop, NPC, enemy or encounter.
- Store an object's geometry, interaction points, route samples and destinations relative to its root, or use explicit anchor actors. Do not reconstruct a placed actor at fixed world coordinates or identify it by a hardcoded map actor name.
- Own related meshes/components within the placeable actor when possible. If a paired gameplay actor must stay separate, make edit-time movement/duplication preserve the pair and runtime ownership remain independent.
- Preserve existing save IDs and references during migration. New placements and editor duplicates need independent identities. PIE duplication must retain the original world's identities.
- Runtime spawners should honor editable Blueprint class references or `/Game/BBSB/Placeables/DA_PlacementClasses`; do not silently bypass the authored presets with a native-class spawn.
- Validate meaningful placement changes by moving, rotating and duplicating an instance, then exercising its real interaction in PIE. Preserve existing quest prerequisites and early-completion behavior.
- Save and back up before bulk asset/level migration. For C++ changes, save, close normally, build normally and reopen; do not use Live Coding in this project.
- See `Docs/BlueprintPlacement.md` for placement, local endpoint, registry and stage wiring details.
