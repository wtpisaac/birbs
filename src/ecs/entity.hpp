#ifndef BIRBS_ECS_ENTITY_H
#define BIRBS_ECS_ENTITY_H 1

#include <cstdint>
#include "quill/DeferredFormatCodec.h"

struct Entity {
    uint64_t entityId;

    Entity(uint32_t id): entityId(id) {}
    Entity() = delete;
 };

template <>
struct fmtquill::formatter<Entity>
{
  constexpr auto parse(format_parse_context& ctx) { return ctx.begin(); }

  auto format(::Entity const& entity, format_context& ctx) const
  {
    return fmtquill::format_to(
        ctx.out(), 
        "Entity(id: {})",
        entity.entityId
    );
  }
};

template <>
struct quill::Codec<Entity> : quill::DeferredFormatCodec<Entity>
{
};
#endif
