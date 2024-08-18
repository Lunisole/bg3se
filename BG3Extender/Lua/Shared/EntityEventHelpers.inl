BEGIN_NS(lua)

LuaEntitySubscriptionId EntityEventHelpers::SubscribeReplication(lua_State* L, EntityHandle entity, ExtComponentType component, RegistryEntry&& hook, std::optional<uint64_t> flags)
{
	auto hooks = State::FromLua(L)->GetReplicationEventHooks();
	if (!hooks) {
		luaL_error(L, "Entity replication events are only available on the server");
	}

	auto replicationType = State::FromLua(L)->GetEntitySystemHelpers()->GetReplicationIndex(component);
	if (!replicationType) {
		luaL_error(L, "No replication events are available for components of type %s", EnumInfo<ExtComponentType>::GetStore().Find((EnumUnderlyingType)component).GetString());
	}

	auto index = hooks->Subscribe(*replicationType, entity, flags ? *flags : 0xffffffffffffffffull, std::move(hook));
	return LuaEntitySubscriptionId((ReplicationEventHandleType << 32) | index);
}

LuaEntitySubscriptionId EntityEventHelpers::Subscribe(lua_State* L, EntityHandle entity, ExtComponentType component, 
	EntityComponentEvent event, EntityComponentEventFlags flags, RegistryEntry&& hook)
{
	auto componentType = State::FromLua(L)->GetEntitySystemHelpers()->GetComponentIndex(component);
	if (!componentType) {
		luaL_error(L, "No events are available for components of type %s", EnumInfo<ExtComponentType>::GetStore().Find((EnumUnderlyingType)component).GetString());
	}

	auto& hooks = State::FromLua(L)->GetComponentEventHooks();
	auto index = hooks.Subscribe(*componentType, entity, event, flags, std::move(hook));
	return LuaEntitySubscriptionId((ComponentEventHandleType << 32) | index);
}

bool EntityEventHelpers::Unsubscribe(lua_State* L, LuaEntitySubscriptionId handle)
{
	switch ((uint64_t)handle >> 32) {
	case ReplicationEventHandleType:
	{
		auto hooks = State::FromLua(L)->GetReplicationEventHooks();
		if (!hooks) {
			luaL_error(L, "Entity events are only available on the server");
		}

		return hooks->Unsubscribe((uint32_t)handle);
	}

	case ComponentEventHandleType:
	{
		return State::FromLua(L)->GetComponentEventHooks().Unsubscribe((uint32_t)handle);
	}

	default:
		OsiWarn("Illegible subscription index");
		return false;
	}
}

END_NS()