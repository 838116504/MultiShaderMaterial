#include <core/class_db.h>
#include "register_types.h"
#include "multi_shader_material.h"

void register_multi_shader_material_types()
{
	ClassDB::register_class<MultiShaderMaterial>();
	MultiShaderMaterial::get_shader_manager() = memnew(MultiShaderMaterial::ShaderManager);
}

void unregister_multi_shader_material_types()
{
	memdelete(MultiShaderMaterial::get_shader_manager());
}
