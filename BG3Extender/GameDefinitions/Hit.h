#pragma once

#include <GameDefinitions/Base/Base.h>
#include <GameDefinitions/EntitySystem.h>
#include <GameDefinitions/Stats/Expression.h>

BEGIN_SE()

struct DamageSums
{
	int TotalDamageDone;
	int TotalHealDone;
	int8_t DamagePercentage;
	int8_t field_9;
	int8_t field_A;
};

struct RerollCondition
{
	uint8_t RollValue;
	bool KeepNew;
};

struct RerollValue
{
	uint8_t RollValue;
	uint8_t RerollType;
};

struct Roll
{
	RollDefinition Roll;
	stats::RollType RollType;
	bool Advantage;
	bool Disadvantage;
	Array<RerollCondition> RerollConditions;
};

struct StatsRollResult
{
	int Total;
	int NaturalRoll;
	int DiscardedDiceTotal;
	RollCritical Critical;
	std::optional<uint32_t> CriticalThreshold;
	Array<RerollValue> RollsCount;
};

struct ResolvedRollBonus
{
	DiceSizeId DiceSize;
	uint8_t NumDice;
	int ResolvedRollBonus;
	[[bg3::legacy(Description)]] TranslatedString SourceName;
};

struct FixedRollBonus
{
	[[bg3::legacy(field_0)]] int RollBonus;
	[[bg3::legacy(Description)]] TranslatedString SourceName;
};

struct StatsRollMetadata
{
	int ProficiencyBonus;
	int RollBonus;
	int HighGroundBonus;
	int LowGroundPenalty;
	[[bg3::legacy(field_10)]] int BaseUnarmedDamage;
	MultiHashMap<AbilityId, int32_t> AbilityBoosts;
	MultiHashMap<SkillId, int32_t> SkillBonuses;
	bool AutoSkillCheckFail;
	bool AutoAbilityCheckFail;
	bool AutoAbilitySavingThrowFail;
	bool HasCustomMetadata;
	bool IsCritical;
	Array<ResolvedRollBonus> ResolvedRollBonuses;
	[[bg3::legacy(ResolvedUnknowns)]] Array<FixedRollBonus> FixedRollBonuses;
};

struct StatsRoll
{
	Roll Roll;
	StatsRollResult Result;
	StatsRollMetadata Metadata;
};

struct StatsExpressionResolved
{
	[[bg3::hidden]]
	StatsExpressionParamEx* CachedStatExpression;
	STDString StatExpression;
	Array<int32_t> IntParams;
	Array<StatsRoll> RollParams;
	Array<DamageType> DamageTypeParams;
	int IntIndex;
	int RollIndex;
	int DamageTypeIndex;
};

struct ConditionRoll
{
	uint8_t DataType;
	//# P_BITMASK(RollType)
	ConditionRollType RollType;
	std::variant<StatsRoll, StatsExpressionResolved> Roll;
	int Difficulty;
	Guid field_120;
	bool field_130;
	AbilityId Ability;
	SkillId Skill;
};

struct DamageModifierMetadata
{
	uint8_t MetadataType;
	int Value;
	DamageType DamageType;
	uint8_t SourceType;
	[[bg3::legacy(Argument)]] std::variant<RollDefinition, int32_t, StatsExpressionResolved> Source;
	[[bg3::legacy(Description)]] TranslatedString SourceName;
	[[bg3::legacy(Description2)]] FixedString SourceId;
};

struct DamageResistance
{
	[[bg3::legacy(Flags)]] uint8_t Type;
	DamageModifierMetadata Meta;
};

struct StatsDamage
{
	RefMap<DamageType, Array<StatsRoll>> DamageRolls;
	Array<DamageModifierMetadata> Modifiers;
	StatsExpressionResolved ConditionRoll;
	Array<DamageModifierMetadata> Modifiers2;
	Array<DamageResistance> Resistances;
	int AdditionalDamage;
	int TotalDamage;
	int FinalDamage;
	RefMap<DamageType, int32_t> TotalDamagePerType;
	RefMap<DamageType, int32_t> FinalDamagePerType;
	[[bg3::legacy(field_D0)]] uint32_t Multiplier;
	[[bg3::legacy(field_D4)]] uint32_t BaseValue;
	[[bg3::legacy(field_D8)]] uint32_t SecondaryValue;
	[[bg3::legacy(field_DC)]] uint8_t DamageMultiplierType;
};

struct HitDesc
{
	struct OverrideEntry
	{
		uint8_t DamageType;
		int OriginalValue;
		int OverriddenValue;
	};

	int TotalDamageDone;
	stats::DeathType DeathType;
	DamageType DamageType;
	CauseType CauseType;
	glm::vec3 ImpactPosition;
	glm::vec3 ImpactDirection;
	float ImpactForce;
	int ArmorAbsorption;
	int LifeSteal;
	// TODO - need to remap DamageFlags
	uint32_t EffectFlags;
	EntityHandle Inflicter;
	EntityHandle InflicterOwner;
	EntityHandle Throwing;
	int StoryActionId;
	HitWith HitWith;
	AbilityId AttackRollAbility;
	AbilityId SaveAbility;
	[[bg3::legacy(field_4F)]] uint8_t SpellAttackType;
	Array<ConditionRoll> ConditionRolls;
	[[bg3::legacy(Results)]] StatsDamage Damage;
	Guid SpellCastGuid;
	FixedString SpellId;
	FixedString field_150;
	EntityHandle field_158;
	uint8_t field_160;
	SpellSchoolId SpellSchool;
	[[bg3::legacy(HitDescFlags)]] uint8_t HealingTypes;
	uint8_t AttackFlags;
	int SpellLevel;
	int SpellPowerLevel;
	int TotalHealDone;
	[[bg3::legacy(field_174)]] float RedirectedDamage;
	int OriginalDamageValue;
	[[bg3::legacy(field_178)]] float FallHeight;
	[[bg3::legacy(field_17C)]] float FallDamageMultiplier;
	[[bg3::legacy(field_180)]] float FallMaxDamage;
	[[bg3::legacy(field_184)]] float FallWeight;
	uint8_t field_188;
	Array<OverrideEntry> OverriddenDamage;
	Array<DamagePair> DamageList;
};

struct HitResultData
{
	Array<int32_t> field_0;
	Array<int32_t> field_10;
	Array<uint8_t> field_20;
	Array<int32_t> field_30;
};

struct HitResult
{
	HitDesc Hit;
	DamageSums DamageSums;
	Array<DamagePair> DamageList;
	HitResultData Results;
	uint32_t NumConditionRolls;
};

END_SE()
