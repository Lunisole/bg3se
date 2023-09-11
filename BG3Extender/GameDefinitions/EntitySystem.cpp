#include "stdafx.h"

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/Symbols.h>
#include <GameDefinitions/EntitySystem.h>
#include <GameDefinitions/GuidResources.h>
#include <GameDefinitions/Components/Components.h>
#include <GameDefinitions/Components/Stats.h>
#include <GameDefinitions/Components/Passives.h>
#include <GameDefinitions/Components/Combat.h>
#include <Extender/ScriptExtender.h>

#undef DEBUG_INDEX_MAPPINGS

#if defined(DEBUG_INDEX_MAPPINGS)
#define DEBUG_IDX(x) std::cout << x << std::endl
#else
#define DEBUG_IDX(x)
#endif

BEGIN_NS(ecs)

void* Query::GetFirstMatchingComponent(std::size_t componentSize)
{
	for (auto const& cls : EntityClasses) {
		if (cls.EntityClass->InstanceToPageMap.size() > 0) {
			auto instPage = cls.EntityClass->InstanceToPageMap.Values[0];
			auto componentIdx = cls.GetComponentIndex(0);
			assert(cls.EntityClass->ComponentPools.size() >= 1);
			return cls.EntityClass->GetComponent(instPage, componentIdx, componentSize);
		}
	}

	return {};
}

Array<void*> Query::GetAllMatchingComponents(std::size_t componentSize)
{
	Array<void*> hits;
	
	for (auto const& cls : EntityClasses) {
		auto componentIdx = cls.GetComponentIndex(0);
		for (auto const& instance : cls.EntityClass->InstanceToPageMap) {
			auto component = cls.EntityClass->GetComponent(instance.Value(), componentIdx, componentSize);
			hits.push_back(component);
		}
	}

	return hits;
}

void* EntityClass::GetComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize) const
{
	auto ref = InstanceToPageMap.Find(entityHandle);
	if (ref) {
		return GetComponent(**ref, type, componentSize);
	} else {
		return nullptr;
	}
}

void* EntityClass::GetComponent(InstanceComponentPointer const& entityPtr, ComponentTypeIndex type, std::size_t componentSize) const
{
	auto compIndex = ComponentTypeToIndex.Find((uint16_t)type.Value());
	if (compIndex) {
		return GetComponent(entityPtr, **compIndex, componentSize);
	} else {
		return nullptr;
	}
}

void* EntityClass::GetComponent(InstanceComponentPointer const& entityPtr, uint8_t componentSlot, std::size_t componentSize) const
{
	auto& page = ComponentPools[entityPtr.PageIndex][componentSlot];
	auto buf = (uint8_t*)page.ComponentBuffer;
	return buf + componentSize * entityPtr.EntryIndex;
}
	
void* EntityWorld::GetRawComponent(EntityHandle entityHandle, ComponentTypeIndex type, std::size_t componentSize)
{
	auto entityClass = GetEntityClass(entityHandle);
	if (entityClass != nullptr) {
		auto component = entityClass->GetComponent(entityHandle, type, componentSize);
		if (component != nullptr) {
			return component;
		}
	}

	auto compPool = Components->ComponentsByType.Find((uint16_t)type);
	if (compPool) {
		auto transientRef = (*compPool)->Find(entityHandle.Handle);
		if (transientRef) {
			return **transientRef;
		}
	}

	auto compPool2 = Components->ComponentsByType2.Find((uint16_t)type);
	if (compPool2) {
		auto transientRef = (*compPool2)->Find(entityHandle.Handle);
		if (transientRef) {
			return **transientRef;
		}
	}

	return nullptr;
}
	
void* EntityWorld::GetRawComponent(char const* nameGuid, ComponentTypeIndex type, std::size_t componentSize)
{
	auto fs = NameGuidToFixedString(nameGuid);
	if (!fs) {
		OsiError("Could not map GUID '" << nameGuid << "' to FixedString");
		return nullptr;
	}

	return GetRawComponent(fs, type, componentSize);
}

void* EntityWorld::GetRawComponent(FixedString const& guid, ComponentTypeIndex type, std::size_t componentSize)
{
	ERR("FIXME!");
	return nullptr;
}

bool EntityWorld::IsValid(EntityHandle entityHandle) const
{
	if (entityHandle.GetType() < 0x40) {
		auto salts = (*EntitySalts)[entityHandle.GetType()];
		if (entityHandle.GetIndex() < salts.NumElements) {
			auto salt = salts.Buckets[entityHandle.GetIndex() >> salts.BitsPerBucket][entityHandle.GetIndex() & ((1 << salts.BitsPerBucket) - 1)];
			return salt.Salt == entityHandle.GetSalt() && salt.Index == entityHandle.GetIndex();
		}
	}

	return false;
}

EntityClass* EntityStore::GetEntityClass(EntityHandle entityHandle) const
{
	auto& componentSalts = Salts.Buckets[entityHandle.GetType()];
	if (entityHandle.GetIndex() < componentSalts.NumElements) {
		auto salt = componentSalts.Buckets[entityHandle.GetIndex() >> componentSalts.BitsPerBucket][entityHandle.GetIndex() & ((1 << componentSalts.BitsPerBucket) - 1)];
		if (salt.Salt == entityHandle.GetSalt()) {
			return EntityClasses[salt.EntityClassIndex];
		}
	}

	return nullptr;
}

EntityClass* EntityWorld::GetEntityClass(EntityHandle entityHandle) const
{
	if (!IsValid(entityHandle)) {
		return nullptr;
	}

	return EntityTypes->GetEntityClass(entityHandle);
}

EntitySystemHelpersBase::EntitySystemHelpersBase()
	: componentIndices_{ UndefinedComponent }, 
	componentSizes_{ 0 },
	handleIndices_{ UndefinedHandle }, 
	queryIndices_{ UndefinedIndex },
	resourceManagerIndices_{ UndefinedIndex }
{}

void EntitySystemHelpersBase::ComponentIndexMappings::Add(int32_t index, IndexSymbolType type)
{
	switch (type) {
	case IndexSymbolType::Replication:
		assert(ReplicationIndex == -1);
		ReplicationIndex = index;
		break;
			
	case IndexSymbolType::Handle:
		assert(HandleIndex == -1);
		HandleIndex = index;
		break;
			
	case IndexSymbolType::Component:
		assert(ComponentIndex == -1);
		ComponentIndex = index;
		break;
			
	case IndexSymbolType::EventComponent:
		assert(EventComponentIndex == -1);
		EventComponentIndex = index;
		break;

	default:
		assert(NumIndices < Indices.size());
		if (NumIndices < Indices.size()) {
			Indices[NumIndices++] = index;
		}
		break;
	}
}

STDString SimplifyComponentName(StringView name)
{
	STDString key{ name };
	if (key.length() > 52 && strncmp(key.c_str(), "class ls::_StringView<char> __cdecl ls::GetTypeName<", 52) == 0) {
		key = key.substr(52);
	}

	if (key.length() > 6 && strncmp(key.c_str(), "class ", 6) == 0) {
		key = key.substr(6);
	}
	else if (key.length() > 7 && strncmp(key.c_str(), "struct ", 7) == 0) {
		key = key.substr(7);
	}

	if (key.length() > 7 && strncmp(key.c_str() + key.size() - 7, ">(void)", 7) == 0) {
		key = key.substr(0, key.size() - 7);
	}

	return key;
}

BitSet<>* EntitySystemHelpersBase::GetReplicationFlags(EntityHandle const& entity, ComponentTypeIndex type)
{
	auto world = GetEntityWorld();
	if (!world || !world->Replication) return nullptr;

	return nullptr;
	/*auto& pools = world->Replication->ComponentPools;
	auto typeId = (int)replicationType;
	if (typeId >= (int)pools.Size()) {
		OsiError("Attempted to fetch replication list for component " << entity << ", but replication pool size is " << pools.Size() << "!");
		return nullptr;
	}

	auto& pool = pools[typeId];
	auto syncFlags = pool.Find(entity);
	if (syncFlags) {
		return *syncFlags;
	} else {
		return nullptr;
	}*/
}

BitSet<>* EntitySystemHelpersBase::GetOrCreateReplicationFlags(EntityHandle const& entity, ComponentTypeIndex replicationType)
{
	auto world = GetEntityWorld();
	if (!world || !world->Replication) return nullptr;

	return nullptr;
	/*auto& pools = world->Replication->ComponentPools;
	auto typeId = (int)replicationType;
	if (typeId >= (int)pools.Size()) {
		OsiError("Attempted to fetch replication list for component " << entity << ", but replication pool size is " << pools.Size() << "!");
		return nullptr;
	}

	auto& pool = pools[typeId];
	auto syncFlags = pool.Find(entity);
	if (syncFlags) {
		return *syncFlags;
	}

	return pool.Set(entity, BitSet<>());*/
}

void EntitySystemHelpersBase::NotifyReplicationFlagsDirtied()
{
	auto world = GetEntityWorld();
	if (!world || !world->Replication) return;

	world->Replication->Dirty = true;
}

void EntitySystemHelpersBase::BindSystemName(StringView name, int32_t systemId)
{
	auto it = systemIndexMappings_.insert(std::make_pair(name, systemId));
	if (systemTypeIdToName_.size() <= systemId) {
		systemTypeIdToName_.resize(systemId + 1);
	}

	systemTypeIdToName_[systemId] = &it.first->first;
}

void EntitySystemHelpersBase::BindQueryName(StringView name, int32_t queryId)
{
	auto it = queryMappings_.insert(std::make_pair(name, queryId));
	if (queryTypeIdToName_.size() <= queryId) {
		queryTypeIdToName_.resize(queryId + 1);
	}

	queryTypeIdToName_[queryId] = &it.first->first;
}

bool EntitySystemHelpersBase::TryUpdateSystemMapping(StringView name, ComponentIndexMappings& mapping)
{
	if (mapping.NumIndices == 1
		&& mapping.ReplicationIndex == -1
		&& mapping.HandleIndex == -1
		&& mapping.ComponentIndex == -1
		&& mapping.EventComponentIndex == -1) {
		if (name.starts_with("ecs::query::spec::Spec<")) {
			BindQueryName(name, mapping.Indices[0]);
		} else {
			BindSystemName(name, mapping.Indices[0]);
		}
		return true;
	}

	return false;
}

void EntitySystemHelpersBase::TryUpdateComponentMapping(StringView name, ComponentIndexMappings& mapping)
{
	std::sort(mapping.Indices.begin(), mapping.Indices.begin() + mapping.NumIndices, std::less<int32_t>());
	DEBUG_IDX("\t" << name << ": ");

	auto totalIndices = mapping.NumIndices
		+ ((mapping.ReplicationIndex != -1) ? 1 : 0)
		+ ((mapping.HandleIndex != -1) ? 1 : 0)
		+ ((mapping.ComponentIndex != -1) ? 1 : 0)
		+ ((mapping.EventComponentIndex != -1) ? 1 : 0);

	if (totalIndices == 1) {
		assert(mapping.ReplicationIndex == -1);
		assert(mapping.HandleIndex == -1);

		if (mapping.ComponentIndex == -1) {
			mapping.ComponentIndex = mapping.Indices[0];
		}
	} else if (totalIndices == 2) {
		// Only HandleIndex and ComponentIndex, no replication
		assert(mapping.ReplicationIndex == -1);
		assert(mapping.HandleIndex == -1);

		if (mapping.EventComponentIndex != -1) {
			DEBUG_IDX("Event component, ignored.");
			return;
		}

		unsigned nextIndex{ 0 };
		if (mapping.ComponentIndex == -1) {
			// Maybe this is a system?
			if (name.find("Component") == StringView::npos) {
				BindSystemName(name, mapping.Indices[0]);
			} else {
				mapping.ComponentIndex = mapping.Indices[nextIndex++];
			}
		}

		mapping.HandleIndex = mapping.Indices[nextIndex++];
	} else if (totalIndices == 3) {
		unsigned nextIndex{ 0 };

		if (mapping.ReplicationIndex == -1) {
			mapping.ReplicationIndex = mapping.Indices[nextIndex++];
		}

		if (mapping.ComponentIndex == -1) {
			mapping.ComponentIndex = mapping.Indices[nextIndex++];
		}

		if (mapping.HandleIndex == -1) {
			mapping.HandleIndex = mapping.Indices[nextIndex++];
		}
	} else {
		WARN("Component with strange configuration: %s", name.data());
		return;
	}

	DEBUG_IDX("Repl " << mapping.ReplicationIndex << ", Handle " << mapping.HandleIndex << ", Comp " << mapping.ComponentIndex);
	IndexMappings indexMapping{ (uint16_t)mapping.HandleIndex, (uint16_t)mapping.ComponentIndex };
	auto it = componentNameToIndexMappings_.insert(std::make_pair(name, indexMapping));
	componentIndexToNameMappings_.insert(std::make_pair(indexMapping.ComponentIndex, &it.first->first));
	handleIndexToNameMappings_.insert(std::make_pair(indexMapping.HandleIndex, &it.first->first));
}

void EntitySystemHelpersBase::UpdateComponentMappings()
{
	if (initialized_) return;

	componentNameToIndexMappings_.clear();
	componentIndexToNameMappings_.clear();
	handleIndexToNameMappings_.clear();
	componentIndexToTypeMappings_.clear();
	handleIndexToTypeMappings_.clear();
	handleIndexToComponentMappings_.clear();
	componentIndices_.fill(UndefinedComponent);
	componentSizes_.fill(0);
	handleIndices_.fill(UndefinedHandle);
	queryIndices_.fill(UndefinedIndex);
	resourceManagerIndices_.fill(UndefinedIndex);

	auto const& symbolMaps = GetStaticSymbols().IndexSymbolToNameMaps;

	std::unordered_map<char const*, ComponentIndexMappings> mappings;
	for (auto const& mapping : symbolMaps) {
		auto it = mappings.find(mapping.second.name);
		if (it == mappings.end()) {
			ComponentIndexMappings newMapping;
			std::fill(newMapping.Indices.begin(), newMapping.Indices.end(), UndefinedIndex);
			newMapping.Add(*mapping.first, mapping.second.type);
			mappings.insert(std::make_pair(mapping.second.name, newMapping));
		} else {
			it->second.Add(*mapping.first, mapping.second.type);
		}
	}

	std::vector<std::pair<STDString, ComponentIndexMappings>> pendingMappings;
	for (auto& map : mappings) {
		auto componentName = SimplifyComponentName(map.first);
		if (!TryUpdateSystemMapping(componentName, map.second)) {
			pendingMappings.push_back({ std::move(componentName), map.second});
		}
	}
	
	for (auto& map : pendingMappings) {
		TryUpdateComponentMapping(map.first, map.second);
	}

#if defined(DEBUG_INDEX_MAPPINGS)
	DEBUG_IDX("COMPONENT MAPPINGS:");

	for (auto const& map : componentNameToIndexMappings_) {
		DEBUG_IDX("\t" << map.first << ": Handle " << map.second.HandleIndex << ", Component " << map.second.ComponentIndex);
	}

	DEBUG_IDX("-------------------------------------------------------");
#endif

#define MAP_COMPONENT(name, ty) \
	MapComponentIndices(name, ty##Component::ComponentType); \
	componentSizes_[(unsigned)ty##Component::ComponentType] = sizeof(ty##Component);

	MAP_COMPONENT("eoc::ActionResourcesComponent", ActionResources);
	MAP_COMPONENT("eoc::ArmorComponent", Armor);
	MAP_COMPONENT("eoc::BaseHpComponent", BaseHp);
	MAP_COMPONENT("eoc::DataComponent", Data);
	MAP_COMPONENT("eoc::exp::ExperienceComponent", Experience);
	MAP_COMPONENT("eoc::HealthComponent", Health);
	MAP_COMPONENT("eoc::PassiveComponent", Passive);
	MAP_COMPONENT("eoc::HearingComponent", Hearing);
	MAP_COMPONENT("eoc::spell::BookComponent", SpellBook);
	MAP_COMPONENT("eoc::StatsComponent", Stats);
	MAP_COMPONENT("eoc::StatusImmunitiesComponent", StatusImmunities);
	MAP_COMPONENT("eoc::SurfacePathInfluencesComponent", SurfacePathInfluences);
	MAP_COMPONENT("eoc::UseComponent", Use);
	MAP_COMPONENT("eoc::ValueComponent", Value);
	MAP_COMPONENT("eoc::WeaponComponent", Weapon);
	MAP_COMPONENT("eoc::WieldingComponent", Wielding);
	MAP_COMPONENT("eoc::CustomStatsComponent", CustomStats);
	MAP_COMPONENT("eoc::BoostConditionComponent", BoostCondition);
	MAP_COMPONENT("eoc::BoostsContainerComponent", BoostsContainer);
	MAP_COMPONENT("eoc::ActionResourceConsumeMultiplierBoostCompnent", ActionResourceConsumeMultiplierBoost);
	MAP_COMPONENT("eoc::combat::ParticipantComponent", CombatParticipant);
	MAP_COMPONENT("eoc::GenderComponent", Gender);
	MAP_COMPONENT("eoc::spell::ContainerComponent", SpellContainer);
	MAP_COMPONENT("eoc::TagComponent", Tag);
	MAP_COMPONENT("eoc::spell::BookPreparesComponent", SpellBookPrepares);
	MAP_COMPONENT("eoc::combat::StateComponent", CombatState);
	MAP_COMPONENT("eoc::TurnBasedComponent", TurnBased);
	MAP_COMPONENT("eoc::TurnOrderComponent", TurnOrder);
	MAP_COMPONENT("ls::TransformComponent", Transform);
	MAP_COMPONENT("eoc::PassiveContainerComponent", PassiveContainer);
	MAP_COMPONENT("eoc::BoostInfoComponent", BoostInfo);
	MAP_COMPONENT("eoc::RelationComponent", Relation);
	MAP_COMPONENT("eoc::CanInteractComponent", CanInteract);
	MAP_COMPONENT("eoc::CanSpeakComponent", CanSpeak);
	MAP_COMPONENT("eoc::OriginComponent", Origin);
	MAP_COMPONENT("ls::LevelComponent", Level);


	MAP_COMPONENT("eoc::BackgroundComponent", Background);
	MAP_COMPONENT("eoc::GodComponent", God);
	MAP_COMPONENT("eoc::LevelUpComponent", LevelUp);
	MAP_COMPONENT("eoc::spell::PlayerPrepareSpellComponent", PlayerPrepareSpell);
	MAP_COMPONENT("eoc::spell::CCPrepareSpellComponent", CCPrepareSpell);
	MAP_COMPONENT("eoc::spell::CastComponent", SpellCast);
	MAP_COMPONENT("eoc::FloatingComponent", Floating);
	MAP_COMPONENT("eoc::VoiceComponent", Voice);
	MAP_COMPONENT("eoc::CustomIconComponent", CustomIcon);
	MAP_COMPONENT("eoc::CharacterCreationStatsComponent", CharacterCreationStats);
	MAP_COMPONENT("eoc::DisarmableComponent", Disarmable);
	MAP_COMPONENT("eoc::rest::ShortRestComponent", ShortRest);
	MAP_COMPONENT("eoc::summon::IsSummonComponent", IsSummon);
	MAP_COMPONENT("eoc::summon::ContainerComponent", SummonContainer);
	MAP_COMPONENT("eoc::StealthComponent", Stealth);
	MAP_COMPONENT("ls::IsGlobalComponent", IsGlobal);
	MAP_COMPONENT("ls::SavegameComponent", Savegame);
	MAP_COMPONENT("eoc::DisabledEquipmentComponent", DisabledEquipment);
	MAP_COMPONENT("eoc::LootingStateComponent", LootingState);
	MAP_COMPONENT("eoc::LootComponent", Loot);
	MAP_COMPONENT("eoc::lock::LockComponent", Lock);
	MAP_COMPONENT("eoc::summon::LifetimeComponent", SummonLifetime);
	MAP_COMPONENT("eoc::InvisibilityComponent", Invisibility);
	MAP_COMPONENT("eoc::IconComponent", Icon);
	MAP_COMPONENT("eoc::hotbar::ContainerComponent", HotbarContainer);
	MAP_COMPONENT("eoc::OriginTagComponent", OriginTag);
	MAP_COMPONENT("eoc::OriginPassivesComponent", OriginPassives);
	MAP_COMPONENT("eoc::GodTagComponent", GodTag);
	MAP_COMPONENT("eoc::ClassTagComponent", ClassTag);
	MAP_COMPONENT("eoc::BackgroundTagComponent", BackgroundTag);
	MAP_COMPONENT("eoc::BackgroundPassivesComponent", BackgroundPassives);
	MAP_COMPONENT("eoc::GlobalShortRestDisabledComponent", GlobalShortRestDisabled);
	MAP_COMPONENT("eoc::GlobalLongRestDisabledComponent", GlobalLongRestDisabled);
	MAP_COMPONENT("eoc::StoryShortRestDisabledComponent", StoryShortRestDisabled);
	MAP_COMPONENT("eoc::FleeCapabilityComponent", FleeCapability);
	MAP_COMPONENT("eoc::CanDoRestComponent", CanDoRest);
	MAP_COMPONENT("eoc::ItemBoostsComponent", ItemBoosts);
	MAP_COMPONENT("eoc::light::ActiveCharacterLightComponent", ActiveCharacterLight);
	MAP_COMPONENT("ls::AnimationSetComponent", AnimationSet);
	MAP_COMPONENT("ls::AnimationBlueprintComponent", AnimationBlueprint);
	MAP_COMPONENT("eoc::CanModifyHealthComponent", CanModifyHealth);
	MAP_COMPONENT("eoc::spell::AddedSpellsComponent", AddedSpells);
	MAP_COMPONENT("eoc::exp::AvailableLevelComponent", AvailableLevel);
	MAP_COMPONENT("eoc::CanBeLootedComponent", CanBeLooted);
	MAP_COMPONENT("eoc::CanDoActionsComponent", CanDoActions);
	MAP_COMPONENT("eoc::CanMoveComponent", CanMove);
	MAP_COMPONENT("eoc::CanSenseComponent", CanSense);
	MAP_COMPONENT("eoc::ConcentrationComponent", Concentration);
	MAP_COMPONENT("eoc::DarknessComponent", Darkness);
	MAP_COMPONENT("eoc::DualWieldingComponent", DualWielding);
	MAP_COMPONENT("eoc::GameObjectVisualComponent", GameObjectVisual);
	MAP_COMPONENT("eoc::InventorySlotComponent", InventorySlot);
	MAP_COMPONENT("eoc::spell::BookCooldownsComponent", SpellBookCooldowns);
	MAP_COMPONENT("eoc::DisplayNameComponent", DisplayName);
	MAP_COMPONENT("eoc::EquipableComponent", Equipable);
	MAP_COMPONENT("eoc::GameplayLightComponent", GameplayLight);
	MAP_COMPONENT("eoc::ProgressionContainerComponent", ProgressionContainer);
	MAP_COMPONENT("eoc::progression::MetaComponent", ProgressionMeta);
	MAP_COMPONENT("eoc::RaceComponent", Race);
	MAP_COMPONENT("eoc::sight::ReplicatedDataComponent", Sight);
	MAP_COMPONENT("eoc::CanTravelComponent", CanTravel);
	MAP_COMPONENT("eoc::CanBeInInventoryComponent", CanBeInInventory);
	MAP_COMPONENT("eoc::MovementComponent", Movement);
	MAP_COMPONENT("eoc::ObjectInteractionComponent", ObjectInteraction);
	MAP_COMPONENT("eoc::PathingComponent", Pathing);
	MAP_COMPONENT("eoc::SteeringComponent", Steering);
	MAP_COMPONENT("eoc::CanDeflectProjectilesComponent", CanDeflectProjectiles);
	MAP_COMPONENT("eoc::spell::LearnedSpellsComponent", LearnedSpells);
	MAP_COMPONENT("eoc::spell::AiConditionsComponent", SpellAiConditions);
	MAP_COMPONENT("ls::ActiveSkeletonSlotsComponent", ActiveSkeletonSlots);
	MAP_COMPONENT("ls::NetComponent", Net);
	MAP_COMPONENT("ls::PhysicsComponent", Physics);
	MAP_COMPONENT("eoc::ftb::ParticipantComponent", FTBParticipant);
	MAP_COMPONENT("eoc::unsheath::InfoComponent", UnsheathInfo);
	MAP_COMPONENT("eoc::approval::Ratings", ApprovalRatings);
	MAP_COMPONENT("eoc::character_creation::AppearanceComponent", CharacterCreationAppearance);
	MAP_COMPONENT("ls::uuid::Component", Uuid);
	MAP_COMPONENT("ls::uuid::ToHandleMappingComponent", UuidToHandleMapping);

	MapQueryIndex("ecs::query::spec::Spec<struct ls::TypeList<struct ls::uuid::ToHandleMappingComponent>,struct ls::TypeList<>,struct ls::TypeList<>,struct ls::TypeList<>,struct ls::TypeList<>,struct ls::TypeList<>,struct ecs::QueryTypePersistentTag,struct ecs::QueryTypeAliveTag>", ExtQueryType::UuidToHandleMapping);

	MapResourceManagerIndex("ls::TagManager", ExtResourceManagerType::Tag);
	MapResourceManagerIndex("eoc::FactionContainer", ExtResourceManagerType::Faction);
	MapResourceManagerIndex("eoc::RaceManager", ExtResourceManagerType::Race);
	MapResourceManagerIndex("eoc::AbilityDistributionPresetManager", ExtResourceManagerType::AbilityDistributionPreset);
	MapResourceManagerIndex("eoc::CompanionPresetManager", ExtResourceManagerType::CompanionPreset);
	MapResourceManagerIndex("eoc::OriginManager", ExtResourceManagerType::Origin);
	MapResourceManagerIndex("eoc::BackgroundManager", ExtResourceManagerType::Background);
	MapResourceManagerIndex("eoc::GodManager", ExtResourceManagerType::God);
	MapResourceManagerIndex("eoc::AbilityListManager", ExtResourceManagerType::AbilityList);
	MapResourceManagerIndex("eoc::SkillListManager", ExtResourceManagerType::SkillList);
	MapResourceManagerIndex("eoc::SpellListManager", ExtResourceManagerType::SpellList);
	MapResourceManagerIndex("eoc::PassiveListManager", ExtResourceManagerType::PassiveList);
	MapResourceManagerIndex("eoc::ProgressionManager", ExtResourceManagerType::Progression);
	MapResourceManagerIndex("eoc::ProgressionDescriptionManager", ExtResourceManagerType::ProgressionDescription);
	MapResourceManagerIndex("eoc::GossipContainer", ExtResourceManagerType::Gossip);
	MapResourceManagerIndex("eoc::ActionResourceTypes", ExtResourceManagerType::ActionResource);
	MapResourceManagerIndex("eoc::ActionResourceGroupManager", ExtResourceManagerType::ActionResourceGroup);
	MapResourceManagerIndex("eoc::EquipmentTypes", ExtResourceManagerType::EquipmentType);
	MapResourceManagerIndex("eoc::VFXContainer", ExtResourceManagerType::VFX);
	MapResourceManagerIndex("eoc::CharacterCreationPresetManager", ExtResourceManagerType::CharacterCreationPreset);
	MapResourceManagerIndex("eoc::CharacterCreationSkinColorManager", ExtResourceManagerType::CharacterCreationSkinColor);
	MapResourceManagerIndex("eoc::CharacterCreationEyeColorManager", ExtResourceManagerType::CharacterCreationEyeColor);
	MapResourceManagerIndex("eoc::CharacterCreationHairColorManager", ExtResourceManagerType::CharacterCreationHairColor);
	MapResourceManagerIndex("eoc::CharacterCreationAccessorySetManager", ExtResourceManagerType::CharacterCreationAccessorySet);
	MapResourceManagerIndex("eoc::CharacterCreationEquipmentIconsManager", ExtResourceManagerType::CharacterCreationEquipmentIcons);
	MapResourceManagerIndex("eoc::CharacterCreationIconSettingsManager", ExtResourceManagerType::CharacterCreationIconSettings);
	MapResourceManagerIndex("eoc::CharacterCreationMaterialOverrideManager", ExtResourceManagerType::CharacterCreationMaterialOverride);
	MapResourceManagerIndex("eoc::CharacterCreationAppearanceMaterialManager", ExtResourceManagerType::CharacterCreationAppearanceMaterial);
	MapResourceManagerIndex("eoc::CharacterCreationPassiveAppearanceManager", ExtResourceManagerType::CharacterCreationPassiveAppearance);
	MapResourceManagerIndex("eoc::CharacterCreationAppearanceVisualManager", ExtResourceManagerType::CharacterCreationAppearanceVisual);
	MapResourceManagerIndex("eoc::CharacterCreationSharedVisualManager", ExtResourceManagerType::CharacterCreationSharedVisual);
	MapResourceManagerIndex("eoc::tutorial::EntriesManager", ExtResourceManagerType::TutorialEntries);
	MapResourceManagerIndex("eoc::FeatManager", ExtResourceManagerType::Feat);
	MapResourceManagerIndex("eoc::FeatDescriptionManager", ExtResourceManagerType::FeatDescription);
	MapResourceManagerIndex("eoc::tutorial::ModalEntriesManager", ExtResourceManagerType::TutorialModalEntries);
	MapResourceManagerIndex("eoc::ClassDescriptions", ExtResourceManagerType::ClassDescription);
	MapResourceManagerIndex("eoc::ColorDefinitions", ExtResourceManagerType::ColorDefinition);
	MapResourceManagerIndex("ls::FlagManager", ExtResourceManagerType::Flag);

#define MAP_BOOST(name) MapComponentIndices("eoc::" #name "Component", ExtComponentType::name); \
	componentSizes_[(unsigned)name##Component::ComponentType] = sizeof(name##Component);

	MAP_BOOST(ArmorClassBoost);
	MAP_BOOST(AbilityBoost);
	MAP_BOOST(RollBonusBoost);
	MAP_BOOST(AdvantageBoost);
	MAP_BOOST(ActionResourceValueBoost);
	MAP_BOOST(CriticalHitBoost);
	MAP_BOOST(AbilityFailedSavingThrowBoost);
	MAP_BOOST(ResistanceBoost);
	MAP_BOOST(WeaponDamageResistanceBoost);
	MAP_BOOST(ProficiencyBonusOverrideBoost);
	MAP_BOOST(JumpMaxDistanceMultiplierBoost);
	MAP_BOOST(HalveWeaponDamageBoost);
	MAP_BOOST(UnlockSpellBoost);
	MAP_BOOST(SourceAdvantageBoost);
	MAP_BOOST(ProficiencyBonusBoost);
	MAP_BOOST(ProficiencyBoost);
	MAP_BOOST(IncreaseMaxHPBoost);
	MAP_BOOST(ActionResourceBlockBoost);
	MAP_BOOST(StatusImmunityBoost);
	MAP_BOOST(UseBoosts);
	MAP_BOOST(TemporaryHPBoost);
	MAP_BOOST(WeightBoost);
	MAP_BOOST(FactionOverrideBoost);
	MAP_BOOST(ActionResourceMultiplierBoost);
	MAP_BOOST(InitiativeBoost);
	MAP_BOOST(DarkvisionRangeBoost);
	MAP_BOOST(DarkvisionRangeMinBoost);
	MAP_BOOST(DarkvisionRangeOverrideBoost);
	MAP_BOOST(AddTagBoost);
	MAP_BOOST(IgnoreDamageThresholdMinBoost);
	MAP_BOOST(SkillBoost);
	MAP_BOOST(WeaponDamageBoost);
	MAP_BOOST(NullifyAbilityBoost);
	MAP_BOOST(RerollBoost);
	MAP_BOOST(DownedStatusBoost);
	MAP_BOOST(WeaponEnchantmentBoost);
	MAP_BOOST(GuaranteedChanceRollOutcomeBoost);
	MAP_BOOST(AttributeBoost);
	MAP_BOOST(GameplayLightBoost);
	MAP_BOOST(DualWieldingBoost);
	MAP_BOOST(SavantBoost);
	MAP_BOOST(MinimumRollResultBoost);
	MAP_BOOST(CharacterWeaponDamageBoost);
	MAP_BOOST(ProjectileDeflectBoost);
	MAP_BOOST(AbilityOverrideMinimumBoost);
	MAP_BOOST(ACOverrideMinimumBoost);
	MAP_BOOST(FallDamageMultiplierBoost);

	initialized_ = true;
}

void EntitySystemHelpersBase::MapComponentIndices(char const* componentName, ExtComponentType type)
{
	auto it = componentNameToIndexMappings_.find(componentName);
	if (it != componentNameToIndexMappings_.end()) {
		componentIndices_[(unsigned)type] = it->second.ComponentIndex;
		handleIndices_[(unsigned)type] = it->second.HandleIndex;
		componentIndexToTypeMappings_.insert(std::make_pair(it->second.ComponentIndex, type));
		handleIndexToTypeMappings_.insert(std::make_pair(it->second.HandleIndex, type));
		handleIndexToComponentMappings_.insert(std::make_pair(it->second.HandleIndex, it->second.ComponentIndex));
	} else {
		// FIXME - disabled until we map the new component list
		// OsiWarn("Could not find index for component: " << componentName);
	}
}

void EntitySystemHelpersBase::MapQueryIndex(char const* name, ExtQueryType type)
{
	auto it = queryMappings_.find(name);
	if (it != queryMappings_.end()) {
		queryIndices_[(unsigned)type] = it->second;
	} else {
		OsiWarn("Could not find index for query: " << name);
	}
}

void EntitySystemHelpersBase::MapResourceManagerIndex(char const* componentName, ExtResourceManagerType type)
{
	auto it = systemIndexMappings_.find(componentName);
	if (it != systemIndexMappings_.end()) {
		resourceManagerIndices_[(unsigned)type] = it->second;
	} else {
		OsiWarn("Could not find index for resource manager: " << componentName);
	}
}

void* EntitySystemHelpersBase::GetRawComponent(char const* nameGuid, ExtComponentType type)
{
	auto world = GetEntityWorld();
	if (!world) {
		return nullptr;
	}

	auto componentIndex = GetComponentIndex(type);
	if (componentIndex) {
		return world->GetRawComponent(nameGuid, *componentIndex, componentSizes_[(unsigned)type]);
	} else {
		return nullptr;
	}
}

void* EntitySystemHelpersBase::GetRawComponent(FixedString const& guid, ExtComponentType type)
{
	auto world = GetEntityWorld();
	if (!world) {
		return nullptr;
	}

	auto componentIndex = GetComponentIndex(type);
	if (componentIndex) {
		return world->GetRawComponent(guid, *componentIndex, componentSizes_[(unsigned)type]);
	} else {
		return nullptr;
	}
}

void* EntitySystemHelpersBase::GetRawComponent(EntityHandle entityHandle, ExtComponentType type)
{
	auto world = GetEntityWorld();
	if (!world) {
		return nullptr;
	}

	auto componentIndex = GetComponentIndex(type);
	if (componentIndex) {
		return world->GetRawComponent(entityHandle, *componentIndex, componentSizes_[(unsigned)type]);
	} else {
		return nullptr;
	}
}

EntityHandle EntitySystemHelpersBase::GetEntityHandle(Guid uuid)
{
	auto query = GetQuery(ExtQueryType::UuidToHandleMapping);
	if (query) {
		auto entityMap = reinterpret_cast<UuidToHandleMappingComponent*>(query->GetFirstMatchingComponent(componentSizes_[(unsigned)ExtComponentType::UuidToHandleMapping]));
		if (entityMap) {
			auto handle = entityMap->Mappings.Find(uuid);
			if (handle) {
				return **handle;
			}
		}
	}

	return {};
}

resource::GuidResourceBankBase* EntitySystemHelpersBase::GetRawResourceManager(ExtResourceManagerType type)
{
	auto index = resourceManagerIndices_[(unsigned)type];
	if (index == UndefinedIndex) {
		OsiError("No resource manager index mapping registered for " << type);
		return {};
	}

	auto defns = GetStaticSymbols().eoc__gGuidResourceManager;
	if (!defns || !*defns) {
		OsiError("Resource definition manager not available yet!");
		return {};
	}

	auto res = (*defns)->Definitions.Find(index);
	if (!res) {
		OsiError("Resource manager missing for " << type);
		return {};
	}

	return **res;
}

Query* EntitySystemHelpersBase::GetQuery(ExtQueryType type)
{
	auto index = queryIndices_[(unsigned)type];
	if (index == UndefinedIndex) {
		OsiError("No query index mapping registered for " << type);
		return {};
	}

	auto world = GetEntityWorld();
	if (!world) {
		return {};
	}

	return &world->Queries.Queries[index];
}

void ServerEntitySystemHelpers::Setup()
{
	UpdateComponentMappings();


	MapComponentIndices("esv::recruit::RecruitedByComponent", ExtComponentType::ServerRecruitedBy);
	MapComponentIndices("esv::GameTimerComponent", ExtComponentType::ServerGameTimer);
	MapComponentIndices("esv::exp::ExperienceGaveOutComponent", ExtComponentType::ServerExperienceGaveOut);
	MapComponentIndices("esv::ReplicationDependencyComponent", ExtComponentType::ServerReplicationDependency);
	MapComponentIndices("esv::summon::IsUnsummoningComponent", ExtComponentType::ServerIsUnsummoning);
	MapComponentIndices("esv::combat::FleeBlockedComponent", ExtComponentType::ServerFleeBlocked);
	MapComponentIndices("esv::ActivationGroupContainerComponent", ExtComponentType::ServerActivationGroupContainer);
	MapComponentIndices("esv::AnubisTagComponent", ExtComponentType::ServerAnubisTag);
	MapComponentIndices("esv::DialogTagComponent", ExtComponentType::ServerDialogTag);
	MapComponentIndices("esv::DisplayNameListComponent", ExtComponentType::ServerDisplayNameList);
	MapComponentIndices("esv::IconListComponent", ExtComponentType::ServerIconList);
	MapComponentIndices("esv::PlanTagComponent", ExtComponentType::ServerPlanTag);
	MapComponentIndices("esv::RaceTagComponent", ExtComponentType::ServerRaceTag);
	MapComponentIndices("esv::TemplateTagComponent", ExtComponentType::ServerTemplateTag);
	MapComponentIndices("esv::passive::ToggledPassivesComponent", ExtComponentType::ServerToggledPassives);
	MapComponentIndices("esv::BoostTagComponent", ExtComponentType::ServerBoostTag);
	MapComponentIndices("esv::TriggerStateComponent", ExtComponentType::ServerTriggerState);
	MapComponentIndices("esv::SafePositionComponent", ExtComponentType::ServerSafePosition);
	MapComponentIndices("esv::AnubisExecutorComponent", ExtComponentType::ServerAnubisExecutor);
	MapComponentIndices("esv::LeaderComponent", ExtComponentType::ServerLeader);
	MapComponentIndices("esv::BreadcrumbComponent", ExtComponentType::ServerBreadcrumb);
	MapComponentIndices("esv::death::DelayDeathCauseComponent", ExtComponentType::ServerDelayDeathCause);
	MapComponentIndices("esv::pickpocket::PickpocketComponent", ExtComponentType::ServerPickpocket);
	MapComponentIndices("esv::ReplicationDependencyOwnerComponent", ExtComponentType::ServerReplicationDependencyOwner);

	MapComponentIndices("ls::StaticPhysicsComponent", ExtComponentType::StaticPhysics);
	MapComponentIndices("ls::anubis::Component", ExtComponentType::Anubis);

	MapComponentIndices("esv::Character", ExtComponentType::ServerCharacter);
	MapComponentIndices("esv::Item", ExtComponentType::ServerItem);
	MapComponentIndices("esv::Projectile", ExtComponentType::ServerProjectile);
	MapComponentIndices("esv::OsirisTagComponent", ExtComponentType::ServerOsirisTag);
	MapComponentIndices("esv::ActiveComponent", ExtComponentType::ServerActive);
}

void ClientEntitySystemHelpers::Setup()
{
	UpdateComponentMappings();

	MapComponentIndices("ls::VisualComponent", ExtComponentType::Visual);

	MapComponentIndices("ecl::Character", ExtComponentType::ClientCharacter);
	MapComponentIndices("ecl::Item", ExtComponentType::ClientItem);
	MapComponentIndices("ecl::Projectile", ExtComponentType::ClientProjectile);
}



EntityWorld* ServerEntitySystemHelpers::GetEntityWorld()
{
	return GetStaticSymbols().GetServerEntityWorld();
}

EntityWorld* ClientEntitySystemHelpers::GetEntityWorld()
{
	return GetStaticSymbols().GetClientEntityWorld();
}

END_NS()
