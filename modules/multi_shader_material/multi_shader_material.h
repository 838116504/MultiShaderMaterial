#ifndef MULTI_SHADER_MATERIAL_H
#define MULTI_SHADER_MATERIAL_H

#include "scene/resources/material.h"
#include "core/os/mutex.h"

class MultiShaderMaterial : public Material
{
	GDCLASS(MultiShaderMaterial, Material);

protected:
	enum ShaderFunc
	{
		VERTEX = 0,
		FRAGMENT,
		LIGHT,
		MAX_FUNC
	};
	typedef struct _ShaderParseData
	{
		bool hasFunc[MAX_FUNC];					// 是否有顶点函数、片段函数、灯函数
		Vector<StringName> params[MAX_FUNC];	// 顶点函数、片段函数、灯函数用的內建变量名字，作为函数參数傳入
		String code;							// 去除shader_type和render_mode的轉換代码
		String mode;							// render_mode的代码
	}ShaderParseData;

public:
	class ShaderManager : public Object
	{
		GDCLASS(ShaderManager, Object);

		typedef struct
		{
			ShaderParseData* parseData;
			Set<MultiShaderMaterial*> users;
		} Data;
		HashMap<String, Data*> m_data;
		Mutex* m_mutex = NULL;

		static String _get_shader_key(const Ref<Shader>& p_shader);
		bool _has_shader_data(const String& p_key) const;
		ShaderParseData* _get_shader_data(const String& p_key) const;
		void _add_shader_data(const String& p_key, ShaderParseData* p_value, MultiShaderMaterial* const p_user);
		void _add_shader_data(const String& p_key, ShaderParseData* p_value, const Set<MultiShaderMaterial*>& p_users);
		void _remove_shader_data(const String& p_key);
		bool _has_shader_data_user(const String& p_key, MultiShaderMaterial* const p_user) const;
		void _add_shader_data_user(const String& p_key, MultiShaderMaterial* const p_user);
		void _remove_shader_data_user(const String& p_key, MultiShaderMaterial* const p_user);
		Set<MultiShaderMaterial*>& _get_shader_data_users(const String& p_key);

		void lock() const;
		void unlock() const;

		static int _get_first_pos(const String& p_code, int p_pos);
		static ShaderParseData* _parse_shader(const Ref<Shader>& p_shader);
		static void MultiShaderMaterial::ShaderManager::_get_node_var_names(ShaderLanguage::Node* p_node, int p_level, Set<StringName>& r_var_names,
			const Map<StringName, ShaderLanguage::BuiltInInfo>& p_builtInVars);
	protected:
		static void _bind_methods();

	public:
		void _shader_changed(const String& p_key, const Ref<Shader>& p_shader);

		ShaderParseData* use_shader(const Ref<Shader>& p_shader, MultiShaderMaterial* const p_material);
		void unuse_shader(const Ref<Shader>& p_shader, MultiShaderMaterial* const p_material);
		bool has_shader_data(const Ref<Shader>& p_shader) const;

		ShaderManager() { m_mutex = Mutex::create(); };
		~ShaderManager() { if (m_mutex) memdelete(m_mutex); };
	};

protected:	
	static ShaderManager* g_shaderManager;

	Vector<Ref<Shader>> m_shaders;
	List<Ref<Shader>> m_usingShaders;
	Ref<Shader> m_shader;

	static void _bind_methods();
	StringName _get_shader_param_name(const StringName& p_name) const;
	bool _set(const StringName& p_name, const Variant& p_value);
	bool _get(const StringName& p_name, Variant& r_ret) const;
	void _get_property_list(List<PropertyInfo>* p_list) const;
	bool property_can_revert(const String& p_name);
	Variant property_get_revert(const String& p_name);

	void _update();
	void _clear();
public:
	static ShaderManager*& get_shader_manager();

	void _shader_changed(const Ref<Shader>& p_shader, ShaderParseData* const p_data);
	void _unusing_shader_changed(const Ref<Shader>& p_shader);
	
	virtual Shader::Mode get_shader_mode() const;

	void set_shaders(const Array& p_shaders);
	Array get_shaders() const;
	void set_shader(const Ref<Shader>& p_shader, int p_pos);		// 修改指定位置的着色器
	Ref<Shader> get_shader(int p_index) const;						// 返回指定位置的着色器
	int get_shader_count() const;									// 返回着色器數量
	void insert_shader(const Ref<Shader>& p_shader, int p_pos);		// 在指定位置插入着色器
	void move_shader(const Ref<Shader>& p_shader, int p_pos);		// 移动着色器到指定位置
	void clear_shader();											// 刪除全部着色器
	void remove_shader(const Ref<Shader>& p_shader);				// 移除指定着色器
	void remove_shader_by_index(int p_index);						// 移除指定位置的着色器
	void set_shader_param(const StringName& p_param, const Variant& p_value, const Ref<Shader>& p_shader);
	Variant get_shader_param(const StringName& p_param, const Ref<Shader>& p_shader) const;

	MultiShaderMaterial();
	virtual ~MultiShaderMaterial();
};

#endif // WEBSOCKET_REGISTER_TYPES_H
