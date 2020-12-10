#include "multi_shader_material.h"
#include "servers/visual/shader_types.h"
#include "modules/regex/regex.h"

MultiShaderMaterial::ShaderManager* MultiShaderMaterial::g_shaderManager = NULL;
const String SHADER_MAIN_FUNC_NAMES[] = { "vertex", "fragment", "light" };

inline String MultiShaderMaterial::ShaderManager::get_shader_key(const Ref<Shader>& p_shader, int p_shaderId)
{
	//if (p_shader->get_path() != "")
	//	return p_shader->get_path();
	return String("_") + String(Variant(p_shader->get_instance_id())) + "_" + Variant(p_shaderId) + "_";
}

inline bool MultiShaderMaterial::ShaderManager::_has_shader_data(const String& p_key) const
{
	return m_data.has(p_key);
}

inline MultiShaderMaterial::ShaderParseData* MultiShaderMaterial::ShaderManager::_get_shader_data(const String& p_key) const
{
	return m_data[p_key]->parseData;
}

inline void MultiShaderMaterial::ShaderManager::_add_shader_data(const String& p_key, ShaderParseData* p_value, MultiShaderMaterial* const p_user)
{
	Data* data = memnew(Data);
	data->parseData = p_value;
	if (p_user)
	{
		data->users.insert(p_user);
	}
	m_data.set(p_key, data);
}

inline void MultiShaderMaterial::ShaderManager::_add_shader_data(const String& p_key, ShaderParseData* p_value, const Set<MultiShaderMaterial*>& p_users)
{
	Data* data = memnew(Data);
	data->parseData = p_value;
	data->users = p_users;
	m_data.set(p_key, data);
}

inline void MultiShaderMaterial::ShaderManager::_remove_shader_data(const String& p_key)
{
	if (!m_data.has(p_key))
		return;

	memdelete(m_data[p_key]->parseData);
	memdelete(m_data[p_key]);
	m_data.erase(p_key);
}

inline bool MultiShaderMaterial::ShaderManager::_has_shader_data_user(const String& p_key, MultiShaderMaterial* const p_user) const
{
	if (!_has_shader_data(p_key))
		return false;

	return m_data[p_key]->users.has(p_user);
}

inline void MultiShaderMaterial::ShaderManager::_add_shader_data_user(const String& p_key, MultiShaderMaterial* const p_user)
{
	if (!m_data.has(p_key))
		return;

	m_data[p_key]->users.insert(p_user);
}

inline void MultiShaderMaterial::ShaderManager::_remove_shader_data_user(const String& p_key, MultiShaderMaterial* const p_user)
{
	if (!m_data.has(p_key))
		return;

	m_data[p_key]->users.erase(p_user);
	if (m_data[p_key]->users.size() <= 0)
	{
		_remove_shader_data(p_key);
	}
}

inline Set<MultiShaderMaterial*>& MultiShaderMaterial::ShaderManager::_get_shader_data_users(const String& p_key)
{
	return m_data[p_key]->users;
}

inline void MultiShaderMaterial::ShaderManager::lock() const
{		
	m_mutex->lock();
}

inline void MultiShaderMaterial::ShaderManager::unlock() const
{
	m_mutex->unlock();
}

// 跳過注釋和空白換行
int MultiShaderMaterial::ShaderManager::_get_first_pos(const String& p_code, int p_pos)
{
	while (p_pos < p_code.length())
	{
		if (p_code[p_pos] == '/' && p_pos > 0 && p_code[p_pos - 1] == '/')
		{
			++p_pos;
			while (p_pos < p_code.length())
			{
				if (p_code[p_pos] == '\n')
				{
					++p_pos;
					break;
				}
				++p_pos;
			}
		}
		else if (p_code[p_pos] == '*' && p_pos > 0 && p_code[p_pos - 1] == '/')
		{
			++p_pos;
			while (p_pos < p_code.length())
			{
				if (p_code[p_pos] == '/' && p_code[p_pos - 1] == '*')
				{
					++p_pos;
					break;
				}
				++p_pos;
			}
		}
		else if (p_code[p_pos] == ' ' || p_code[p_pos] == '\r' || p_code[p_pos] == '\n' || p_code[p_pos] == '\t' || p_code[p_pos] == '/')
			++p_pos;
		else
			break;
	}

	return p_pos;
}


MultiShaderMaterial::ShaderParseData* MultiShaderMaterial::ShaderManager::_parse_shader(const Ref<Shader>& p_shader, const String& p_head)
{
	String code = p_shader->get_code();
	VS::ShaderMode type = (VS::ShaderMode)p_shader->get_mode();

	ShaderLanguage parser;
	Error err = parser.compile(code, ShaderTypes::get_singleton()->get_functions(type), ShaderTypes::get_singleton()->get_modes(type), ShaderTypes::get_singleton()->get_types());
	if (err != OK)
		return NULL;

	// 移除shader_type代码
	int pos = _get_first_pos(code, 0);
	while (pos < code.length())
	{
		if (code[pos] == ';')
		{
			++pos;
			break;
		}
		++pos;
	}
	if (pos >= code.length())
		return NULL;
	code.erase(0, pos);

	// 获取render_mode及移除render_mode代码
	int find = code.find("render_mode");
	String mode;
	if (find >= 0)
	{
		pos = find + strlen("render_mode");
		while (pos < code.length())
		{
			if (code[pos] == ';')
			{
				++pos;
				break;
			}
			++pos;
		}
		if (pos <= code.length())
		{
			mode = code.substr(find, pos - find);
			code.erase(find, mode.length());
		}
	}

	MultiShaderMaterial::ShaderParseData* ret = memnew(MultiShaderMaterial::ShaderParseData);
	ret->mode = mode;
	for (int i = 0; i < MAX_FUNC; ++i)
	{
		ret->hasFunc[i] = false;
	}

	int len = parser.get_shader()->functions.size();
	Ref<RegEx> regex = memnew(RegEx);
	String varPrev = "[^a-zA-Z0-9_]?";
	String varNext = "[^a-zA-Z0-9_\\(]?";
	Array result;
	int start;
	
	String fn;
	bool processed;
	for (int i = 0; i < len; ++i)
	{
		fn = parser.get_shader()->functions[i].name;
		regex->compile(varPrev + fn + "\\(");
		result = regex->search_all(code);
		processed = false;
		for (int f = 0; f < MAX_FUNC; ++f)
		{
			if (fn == SHADER_MAIN_FUNC_NAMES[f])
			{
				ret->hasFunc[f] = true;
				auto builtInVars = ShaderTypes::get_singleton()->get_functions(type)[SHADER_MAIN_FUNC_NAMES[f]].built_ins;
				Set<StringName> varNames;
				_get_node_var_names(parser.get_shader()->functions[i].function->body, 0, varNames, builtInVars);
				String param;
				for (auto e = varNames.front(); e; e = e->next())
				{
					ret->params[f].push_back(e->get());
					if (param != "")
						param += ", ";

					if (!builtInVars[e->get()].constant)
						param += String("inout ");
					param += ShaderLanguage::get_datatype_name(builtInVars[e->get()].type) + " p_" + e->get();
				}

				// 內建变量名字加p_前綴，使用了的內建变量作为函数參数加入，函数名字加前綴
				for (int i = result.size() - 1; i >= 0; --i)
				{
					start = dynamic_cast<RegExMatch*>(result[i].operator Object * ())->get_start(0);
					if (!((code[start] >= 'a' && code[start] <= 'z') || (code[start] >= 'A' && code[start] <= 'Z') || code[start] == '_'))
					{
						++start;
					}

					if (varNames.size() > 0)
					{
						int blockStart = code.find("{", start);

						if (blockStart > 0)
						{
							int lv = 1;
							int pos = blockStart + 1;
							int blockEnd = -1;
							while (pos < code.length())
							{
								if (code[pos] == '{')
								{
									lv += 1;
								}
								else if (code[pos] == '}')
								{
									lv -= 1;
									if (lv == 0)
									{
										blockEnd = pos;
										break;
									}
								}
								++pos;
							}
							if (blockEnd > 0)
							{
								for (auto e = varNames.front(); e; e = e->next())
								{
									Ref<RegEx> reg = memnew(RegEx);
									reg->compile(varPrev + e->get() + varNext);
									Array r = reg->search_all(code, blockStart, blockEnd);
									int s;
									for (int j = r.size() - 1; j >= 0; --j)
									{
										s = dynamic_cast<RegExMatch*>(r[j].operator Object * ())->get_start(0);
										if (!((code[s] >= 'a' && code[s] <= 'z') || (code[s] >= 'A' && code[s] <= 'Z') || code[s] == '_'))
										{
											++s;
										}
										code = code.insert(s, "p_");
										blockEnd += 2;
									}
								}
							}
						}
					}

					int funcLen = SHADER_MAIN_FUNC_NAMES[f].length() + 1;
					code = code.insert(start + funcLen, param);
					code = code.insert(start, p_head);
				}
				processed = true;
				break;
			}
		}
		
		if (!processed)
		{
			for (int i = result.size() - 1; i >= 0; --i)
			{
				start = dynamic_cast<RegExMatch*>(result[i].operator Object * ())->get_start(0);
				if (!((code[start] >= 'a' && code[start] <= 'z') || (code[start] >= 'A' && code[start] <= 'Z') || code[start] == '_'))
				{
					++start;
				}
				code = code.insert(start, p_head);
			}
		}
	}

	processed = false;
	for (int f = 0; f < MAX_FUNC; ++f)
	{
		if (ret->hasFunc[f])
		{
			processed = true;
			break;
		}
	}
	if (!processed)
		return NULL;

	Set<StringName> vars;
	for (auto e = parser.get_shader()->varyings.front(); e; e = e->next())
	{
		vars.insert(e->key());
	}
	for (auto e = parser.get_shader()->constants.front(); e; e = e->next())
	{
		vars.insert(e->key());
	}
	for (auto e = parser.get_shader()->uniforms.front(); e; e = e->next())
	{
		vars.insert(e->key());
	}

	for (auto e = vars.front(); e; e = e->next())
	{
		regex->compile(varPrev + e->get() + varNext);
		result = regex->search_all(code);
		for (int i = result.size() - 1; i >= 0; i--)
		{
			start = dynamic_cast<RegExMatch*>(result[i].operator Object * ())->get_start(0);
			if (!((code[start] >= 'a' && code[start] <= 'z') || (code[start] >= 'A' && code[start] <= 'Z') || code[start] == '_'))
			{
				++start;
			}
			code = code.insert(start, p_head);
		}
	}

	ret->code = code;
	return ret;
}

void MultiShaderMaterial::ShaderManager::_get_node_var_names(ShaderLanguage::Node* p_node, int p_level, Set<StringName>& r_var_names,
	const Map<StringName, ShaderLanguage::BuiltInInfo>& p_builtInVars)
{
	switch (p_node->type)
	{
	case ShaderLanguage::Node::TYPE_BLOCK:
	{
		ShaderLanguage::BlockNode* bnode = (ShaderLanguage::BlockNode*)p_node;

		for (int i = 0; i < bnode->statements.size(); i++)
		{
			_get_node_var_names(bnode->statements[i], p_level, r_var_names, p_builtInVars);
		}

	} break;
	case ShaderLanguage::Node::TYPE_VARIABLE:
	{
		ShaderLanguage::VariableNode* vnode = (ShaderLanguage::VariableNode*)p_node;

		if (p_builtInVars.has(vnode->name))
			r_var_names.insert(vnode->name);
	} break;
	
	case ShaderLanguage::Node::TYPE_ARRAY:
	{
		ShaderLanguage::ArrayNode* anode = (ShaderLanguage::ArrayNode*)p_node;

		if (p_builtInVars.has(anode->name))
			r_var_names.insert(anode->name);
		

		if (anode->call_expression != NULL)
		{
			_get_node_var_names(anode->call_expression, p_level, r_var_names, p_builtInVars);
		}

		if (anode->index_expression != NULL)
		{
			_get_node_var_names(anode->index_expression, p_level, r_var_names, p_builtInVars);
		}

	} break;
	case ShaderLanguage::Node::TYPE_VARIABLE_DECLARATION: {
		ShaderLanguage::VariableDeclarationNode* vdnode = (ShaderLanguage::VariableDeclarationNode*)p_node;

		for (int i = 0; i < vdnode->declarations.size(); i++) {
			if (vdnode->declarations[i].initializer) {
				_get_node_var_names(vdnode->declarations[i].initializer, p_level, r_var_names, p_builtInVars);
			}
		}
	} break;
	case ShaderLanguage::Node::TYPE_ARRAY_DECLARATION: {

		ShaderLanguage::ArrayDeclarationNode* adnode = (ShaderLanguage::ArrayDeclarationNode*)p_node;

		for (int i = 0; i < adnode->declarations.size(); i++) {
			int sz = adnode->declarations[i].initializer.size();
			if (sz > 0) {
				for (int j = 0; j < sz; j++) {
					_get_node_var_names(adnode->declarations[i].initializer[j], p_level, r_var_names, p_builtInVars);
				}
			}
		}
	} break;
	case ShaderLanguage::Node::TYPE_OPERATOR:
	{
		ShaderLanguage::OperatorNode* onode = (ShaderLanguage::OperatorNode*)p_node;

		switch (onode->op)
		{
		case ShaderLanguage::OP_ASSIGN:
		case ShaderLanguage::OP_ASSIGN_ADD:
		case ShaderLanguage::OP_ASSIGN_SUB:
		case ShaderLanguage::OP_ASSIGN_MUL:
		case ShaderLanguage::OP_ASSIGN_DIV:
		case ShaderLanguage::OP_ASSIGN_SHIFT_LEFT:
		case ShaderLanguage::OP_ASSIGN_SHIFT_RIGHT:
		case ShaderLanguage::OP_ASSIGN_MOD:
		case ShaderLanguage::OP_ASSIGN_BIT_AND:
		case ShaderLanguage::OP_ASSIGN_BIT_OR:
		case ShaderLanguage::OP_ASSIGN_BIT_XOR:
		case ShaderLanguage::OP_INDEX:
			_get_node_var_names(onode->arguments[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(onode->arguments[1], p_level, r_var_names, p_builtInVars);
			break;
		case ShaderLanguage::OP_BIT_INVERT:
		case ShaderLanguage::OP_NEGATE:
		case ShaderLanguage::OP_NOT:
		case ShaderLanguage::OP_DECREMENT:
		case ShaderLanguage::OP_INCREMENT:
		case ShaderLanguage::OP_POST_DECREMENT:
		case ShaderLanguage::OP_POST_INCREMENT:
			_get_node_var_names(onode->arguments[0], p_level, r_var_names, p_builtInVars);
			break;
		case ShaderLanguage::OP_CALL:
		case ShaderLanguage::OP_CONSTRUCT:
		{
			ERR_FAIL_COND(onode->arguments[0]->type != ShaderLanguage::Node::TYPE_VARIABLE);

			ShaderLanguage::VariableNode* vnode = (ShaderLanguage::VariableNode*)onode->arguments[0];

			/*if (onode->op == ShaderLanguage::OP_CALL) {
				if (p_builtInVars.has(vnode->name))
				{
					r_var_names.insert(vnode->name);
				}
			}*/
			for (int i = 1; i < onode->arguments.size(); i++) {
				_get_node_var_names(onode->arguments[i], p_level, r_var_names, p_builtInVars);
			}
		} break;
		case ShaderLanguage::OP_SELECT_IF:
			_get_node_var_names(onode->arguments[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(onode->arguments[1], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(onode->arguments[2], p_level, r_var_names, p_builtInVars);
			break;
		default:
			_get_node_var_names(onode->arguments[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(onode->arguments[1], p_level, r_var_names, p_builtInVars);
			break;
		}

	} break;
	case ShaderLanguage::Node::TYPE_CONTROL_FLOW: {
		ShaderLanguage::ControlFlowNode* cfnode = (ShaderLanguage::ControlFlowNode*)p_node;
		if (cfnode->flow_op == ShaderLanguage::FLOW_OP_IF)
		{
			_get_node_var_names(cfnode->expressions[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(cfnode->blocks[0], p_level + 1, r_var_names, p_builtInVars);
			if (cfnode->blocks.size() == 2)
			{
				_get_node_var_names(cfnode->blocks[1], p_level + 1, r_var_names, p_builtInVars);
			}
		}
		else if (cfnode->flow_op == ShaderLanguage::FLOW_OP_SWITCH)
		{
			_get_node_var_names(cfnode->expressions[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(cfnode->blocks[0], p_level + 1, r_var_names, p_builtInVars);
		}
		else if (cfnode->flow_op == ShaderLanguage::FLOW_OP_CASE)
		{
			_get_node_var_names(cfnode->expressions[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(cfnode->blocks[0], p_level + 1, r_var_names, p_builtInVars);
		}
		else if (cfnode->flow_op == ShaderLanguage::FLOW_OP_DEFAULT)
		{
			_get_node_var_names(cfnode->blocks[0], p_level + 1, r_var_names, p_builtInVars);
		}
		else if (cfnode->flow_op == ShaderLanguage::FLOW_OP_DO)
		{
			_get_node_var_names(cfnode->blocks[0], p_level + 1, r_var_names, p_builtInVars);
			_get_node_var_names(cfnode->expressions[0], p_level, r_var_names, p_builtInVars);

		}
		else if (cfnode->flow_op == ShaderLanguage::FLOW_OP_WHILE)
		{
			_get_node_var_names(cfnode->expressions[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(cfnode->blocks[0], p_level + 1, r_var_names, p_builtInVars);
		}
		else if (cfnode->flow_op == ShaderLanguage::FLOW_OP_FOR)
		{
			_get_node_var_names(cfnode->blocks[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(cfnode->expressions[0], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(cfnode->expressions[1], p_level, r_var_names, p_builtInVars);
			_get_node_var_names(cfnode->blocks[1], p_level + 1, r_var_names, p_builtInVars);

		}
		else if (cfnode->flow_op == ShaderLanguage::FLOW_OP_RETURN)
		{
			if (cfnode->expressions.size())
			{
				_get_node_var_names(cfnode->expressions[0], p_level, r_var_names, p_builtInVars);
			}
		}
	} break;
	case ShaderLanguage::Node::TYPE_MEMBER:
	{
		ShaderLanguage::MemberNode* mnode = (ShaderLanguage::MemberNode*)p_node;
		_get_node_var_names(mnode->owner, p_level, r_var_names, p_builtInVars);
	} break;
	case ShaderLanguage::Node::TYPE_FUNCTION:
	{
	} break;
	}
}


void MultiShaderMaterial::ShaderManager::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("_shader_changed", "shader"), &MultiShaderMaterial::ShaderManager::_shader_changed);
}

// 着色器changed信号處理︰重新解析着色器，通知所有使用該着色器的Material
void MultiShaderMaterial::ShaderManager::_shader_changed(const Ref<Shader>& p_shader)
{
	lock();
	String keyPrefix = String("_") + Variant(p_shader->get_instance_id()) + "_";
	Vector<String> keys;
	Vector<ShaderParseData*> newData;
	List<String> keyList;
	m_data.get_key_list(&keyList);
	for (auto e = keyList.front(); e; e = e->next())
	{
		if (e->get().begins_with(keyPrefix))
		{
			keys.push_back(e->get());
			newData.push_back(_parse_shader(p_shader, e->get()));
		}
	}

	if (keys.size() <= 0)
	{
		if (p_shader->is_connected("changed", this, "_shader_changed"))
		{
			Ref<Shader> s = p_shader;
			s->disconnect("changed", this, "_shader_changed");
		}
		unlock();
		return;
	}

	if (newData[0])
	{
		Set<MultiShaderMaterial*> users;
		for (int i = 0; i < keys.size(); ++i)
		{
			auto u = _get_shader_data_users(keys[i]);
			_add_shader_data(keys[i], newData[i], u);

			for (auto e = u.front(); e; e = e->next())
			{
				users.insert(e->get());
			}
		}
		for (auto e = users.front(); e; e = e->next())
		{
			e->get()->_shader_changed(p_shader, true);
		}
	}
	else
	{
		Set<MultiShaderMaterial*> users;
		for (int i = 0; i < keys.size(); ++i)
		{
			_remove_shader_data(keys[i]);
			auto u = _get_shader_data_users(keys[i]);
			for (auto e = u.front(); e; e = e->next())
			{
				users.insert(e->get());
			}
		}
		for (auto e = users.front(); e; e = e->next())
		{
			e->get()->_shader_changed(p_shader, false);
		}
		if (p_shader->is_connected("changed", this, "_shader_changed"))
		{
			Ref<Shader> s = p_shader;
			s->disconnect("changed", this, "_shader_changed");
		}
	}
	
	unlock();
}

// 获取指定着色器解析数据和为指定Material注冊使用
MultiShaderMaterial::ShaderParseData* MultiShaderMaterial::ShaderManager::use_shader(const Ref<Shader>& p_shader, int p_shaderId, MultiShaderMaterial* const p_material)
{
	lock();
	String key = get_shader_key(p_shader, p_shaderId);
	ShaderParseData* data;
	if (!_has_shader_data(key))
	{
		data = _parse_shader(p_shader, key);
		if (data == NULL)
		{
			unlock();
			return NULL;
		}
			
		_add_shader_data(key, data, p_material);
		if (!p_shader->is_connected("changed", this, "_shader_changed"))
		{
			Vector<Variant> param;
			param.push_back(p_shader);
			Ref<Shader> s = p_shader;
			s->connect("changed", this, "_shader_changed", param);
		}
	}
	else
	{
		data = _get_shader_data(key);
		_add_shader_data_user(key, p_material);
	}
	unlock();
	return data;
}

// 指定Material注銷使用指定着色器(不再通知修改，如果沒人用就釋放解析数据)
void MultiShaderMaterial::ShaderManager::unuse_shader(const Ref<Shader>& p_shader, int p_shaderId, MultiShaderMaterial* const p_material)
{
	lock();
	String key = get_shader_key(p_shader, p_shaderId);
	if (!_has_shader_data_user(key, p_material))
	{
		unlock();
		return;
	}

	_remove_shader_data_user(key, p_material);

	unlock();
}

bool MultiShaderMaterial::ShaderManager::has_shader_data(const Ref<Shader>& p_shader, int p_shaderId) const
{
	lock();
	bool ret = _has_shader_data(get_shader_key(p_shader, p_shaderId));
	unlock();
	return ret;
}

void MultiShaderMaterial::_bind_methods()
{
	ClassDB::bind_method(D_METHOD("_unusing_shader_changed", "shader"), &MultiShaderMaterial::_unusing_shader_changed);
	ClassDB::bind_method(D_METHOD("set_shaders", "shaders"), &MultiShaderMaterial::set_shaders);
	ClassDB::bind_method(D_METHOD("get_shaders"), &MultiShaderMaterial::get_shaders);
	ClassDB::bind_method(D_METHOD("set_shader", "shader", "pos"), &MultiShaderMaterial::set_shader, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_shader", "index"), &MultiShaderMaterial::get_shader, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_shader_count"), &MultiShaderMaterial::get_shader_count);
	ClassDB::bind_method(D_METHOD("insert", "shader", "pos"), &MultiShaderMaterial::insert_shader, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("move", "index", "pos"), &MultiShaderMaterial::move_shader);
	ClassDB::bind_method(D_METHOD("clear"), &MultiShaderMaterial::clear_shader);
	ClassDB::bind_method(D_METHOD("remove", "index"), &MultiShaderMaterial::remove_shader);
	ClassDB::bind_method(D_METHOD("set_shader_param", "param", "value", "index"), &MultiShaderMaterial::set_shader_param, DEFVAL(-1));
	ClassDB::bind_method(D_METHOD("get_shader_param", "param", "index"), &MultiShaderMaterial::get_shader_param, DEFVAL(-1));

	ADD_PROPERTY(PropertyInfo(Variant::ARRAY, "shaders", PROPERTY_HINT_NONE, String(Variant(Variant::OBJECT)) + "/" + Variant(PROPERTY_HINT_RESOURCE_TYPE) + ":Shader",
		PROPERTY_USAGE_DEFAULT), "set_shaders", "get_shaders");
}

StringName MultiShaderMaterial::_get_shader_param_name(const StringName& p_name) const
{
	if (m_usingShaders.size() == 1)
	{
		StringName pr = m_shader->remap_param(p_name);
		if (!pr) {
			String n = p_name;
			if (n.find("param/") == 0) { //backwards compatibility
				pr = n.substr(6, n.length());
			}
			if (n.find("shader_param/") == 0) { //backwards compatibility
				pr = n.replace_first("shader_param/", "");
			}
		}
		return pr;
	}

	String name = p_name;
	int find = name.find("_");
	if (find < 0)
		return StringName();

	int i = Variant(name.substr(0, find));
	if (i < 0 || i >= m_shaders.size() || m_shaders[i].is_null())
		return StringName();

	return g_shaderManager->get_shader_key(m_shaders[i], m_shaderIds[i]) + name.substr(find + 1 + strlen("shader_param/"));
}

bool MultiShaderMaterial::_set(const StringName& p_name, const Variant& p_value)
{
	if (m_shader.is_valid())
	{
		StringName pr = _get_shader_param_name(p_name);

		if (pr)
		{
			VisualServer::get_singleton()->material_set_param(_get_material(), pr, p_value);
			return true;
		}
	}

	return false;
}

bool MultiShaderMaterial::_get(const StringName& p_name, Variant& r_ret) const
{
	if (m_shader.is_valid())
	{
		StringName pr = _get_shader_param_name(p_name);

		if (pr)
		{
			r_ret = VisualServer::get_singleton()->material_get_param(_get_material(), pr);
			return true;
		}
	}

	return false;
}

void MultiShaderMaterial::_get_property_list(List<PropertyInfo>* p_list) const
{
	if (m_shader.is_valid())
	{
		m_shader->get_param_list(p_list);
		if (m_usingShaders.size() > 1)
		{
			Ref<RegEx> regex = memnew(RegEx);
			regex->compile("_(?<id>[0-9]*)_(?<id2>[0-9]*)_");
			ObjectID id;
			int id2;
			Ref<RegExMatch> match;
			int len;
			for (auto e = p_list->front(); e;)
			{
				auto next = e->next();
				match = regex->search(e->get().name);
				if (match.is_null())
				{
					p_list->erase(e);
					e = next;
					continue;
				}
				id = Variant(match->get_string("id"));
				id2 = Variant(match->get_string("id2"));
				len = m_shaders.size();
				for (int i = 0; i < len; ++i)
				{
					if (m_shaders[i].is_valid() && id == m_shaders[i]->get_instance_id() && id2 == m_shaderIds[i])
					{
						if (e->get().name.find("param/") == 0)
						{ //backwards compatibility
							e->get().name = e->get().name.replace_first("param/", "shader_param/");
						}
						else if (e->get().name.find("shader_param/") != 0)
						{
							e->get().name = "shader_param/" + e->get().name;
						}
						e->get().name = Variant(i).operator String() + "_" + regex->sub(e->get().name, "");

						break;
					}
				}
				e = next;
			}
		}		
	}
}

bool MultiShaderMaterial::property_can_revert(const String& p_name)
{
	if (m_shader.is_valid())
	{
		StringName pr = _get_shader_param_name(p_name);
		if (pr)
		{
			Variant default_value = VisualServer::get_singleton()->material_get_param_default(_get_material(), pr);
			Variant current_value;
			_get(p_name, current_value);
			return default_value.get_type() != Variant::NIL && default_value != current_value;
		}
	}
	return false;
}

Variant MultiShaderMaterial::property_get_revert(const String& p_name)
{
	Variant r_ret;
	if (m_shader.is_valid())
	{
		StringName pr = _get_shader_param_name(p_name);
		if (pr)
		{
			r_ret = VisualServer::get_singleton()->material_get_param_default(_get_material(), pr);
		}
	}
	return r_ret;
}

MultiShaderMaterial::ShaderManager*& MultiShaderMaterial::get_shader_manager()
{
	return g_shaderManager;
}

void MultiShaderMaterial::_shader_changed(const Ref<Shader>& p_shader, bool p_isParse)
{
	if (m_usingShaders.size() == 0 && !p_isParse)
		return;

	if (!m_usingShaders.find(p_shader) && p_shader->get_mode() != m_usingShaders.front()->get()->get_mode())
		return;

	_update();
}

void MultiShaderMaterial::_unusing_shader_changed(const Ref<Shader>& p_shader)
{
	if (p_shader.is_null())
		return;
	int find = m_shaders.find(p_shader);
	if (find < 0)
		return;

	if (g_shaderManager->use_shader(p_shader, m_shaderIds[find], this))
		_update();
}

void MultiShaderMaterial::_update()
{
	Shader::Mode type;
	int count = m_shaders.size();
	m_usingShaders.clear();
	Vector<int> usingShaderId;
	for (int i = 0; i < count; ++i)
	{
		if (m_shaders[i].is_null())
			continue;

		if (!g_shaderManager->use_shader(m_shaders[i], m_shaderIds[i], this))
		{
			if (!m_shaders[i]->is_connected("changed", this, "_unusing_shader_changed"))
			{
				Vector<Variant> param;
				param.push_back(m_shaders[i]);
				m_shaders.ptrw()[i]->connect("changed", this, "_unusing_shader_changed", param);
			}
			continue;
		}

		if (m_shaders[i]->is_connected("changed", this, "_unusing_shader_changed"))
		{
			m_shaders.ptrw()[i]->disconnect("changed", this, "_unusing_shader_changed");
		}

		if (m_usingShaders.size() == 0)
		{
			m_usingShaders.push_back(m_shaders[i]);
			usingShaderId.push_back(m_shaderIds[i]);
			type = m_shaders[i]->get_mode();
		}
		else if (type == m_shaders[i]->get_mode())
		{
			m_usingShaders.push_back(m_shaders[i]);
			usingShaderId.push_back(m_shaderIds[i]);
		}
	}

	if (m_usingShaders.size() <= 0)
	{
		if (m_shader.is_valid())
		{
			m_shader.unref();
			VS::get_singleton()->material_set_shader(_get_material(), RID());
			_change_notify();
			emit_changed();
		}
		else
		{
			_change_notify("shaders");
		}
	}
	else if (m_usingShaders.size() == 1)
	{
		if (m_shader != m_usingShaders.front()->get())
		{
			m_shader = m_usingShaders.front()->get();
			VS::get_singleton()->material_set_shader(_get_material(), m_shader->get_rid());
			_change_notify();
			emit_changed();
		}
		else
		{
			_change_notify("shaders");
		}
	}
	else
	{
		String code = "shader_type " + ShaderLanguage::get_shader_type(m_usingShaders.front()->get()->get_code()) + ";\n";
		code += g_shaderManager->use_shader(m_usingShaders.back()->get(), usingShaderId[usingShaderId.size()-1], this)->mode;

		String funcCode[MAX_FUNC];
		ShaderParseData* pData;
		String head;
		int i = 0;
		for (auto e = m_usingShaders.front(); e; e = e->next(), ++i)
		{
			head = g_shaderManager->get_shader_key(e->get(), usingShaderId[i]);
			pData = g_shaderManager->use_shader(e->get(), usingShaderId[i], this);
			code += pData->code;
			for (int f = 0; f < MAX_FUNC; ++f)
			{
				if (pData->hasFunc[f])
				{
					funcCode[f] += String("\t") + head + SHADER_MAIN_FUNC_NAMES[f] + "(";
					int len = pData->params[f].size();
					for (int i = 0; i < len; ++i)
					{
						if (i > 0)
							funcCode[f] += ", ";
						funcCode[f] += pData->params[f][i];
					}
					funcCode[f] += ");\n";
				}
			}
		}

		for (int f = 0; f < MAX_FUNC; ++f)
		{
			if (funcCode[f] != "")
			{
				code += "\nvoid " + SHADER_MAIN_FUNC_NAMES[f] + "() {\n" + funcCode[f] + "}\n";
			}
		}

		if (m_shader.is_valid())
			m_shader.unref();

		m_shader.instance();
		m_shader->set_code(code);
		VS::get_singleton()->material_set_shader(_get_material(), m_shader->get_rid());
		_change_notify();
		emit_changed();
	}
}

Shader::Mode MultiShaderMaterial::get_shader_mode() const
{
	if (m_shader.is_valid())
		return m_shader->get_mode();
	else
		return Shader::MODE_SPATIAL;
}

void MultiShaderMaterial::_clear()
{
	int count = m_shaders.size();
	for (int i = 0; i < count; ++i)
	{
		if (m_shaders[i].is_null())
			continue;

		if (g_shaderManager->has_shader_data(m_shaders[i], m_shaderIds[i]))
			g_shaderManager->unuse_shader(m_shaders[i], m_shaderIds[i], this);
		if (m_shaders[i]->is_connected("changed", this, "_unusing_shader_changed"))
		{
			m_shaders.ptrw()[i]->disconnect("changed", this, "_unusing_shader_changed");
		}
	}
	m_shaders.clear();
	m_shaderIds.clear();

}

void MultiShaderMaterial::set_shaders(const Array& p_shaders)
{
	_clear();
	int len = p_shaders.size();
	Ref<Shader> shader;
	RefPtr refPtr;
	Map<Ref<Shader>, int> shaderCount;
	for (int i = 0; i < len; ++i)
	{
		shader = p_shaders[i];
		m_shaders.push_back(shader);
		if (shaderCount.has(shader))
		{
			m_shaderIds.push_back(shaderCount[shader]);
			++shaderCount[shader];
		}
		else
		{
			m_shaderIds.push_back(0);
			shaderCount[shader] = 1;
		}
	}

	_update();
}

Array MultiShaderMaterial::get_shaders() const
{
	Array ret;
	int len = m_shaders.size();
	for (int i = 0; i < len; ++i)
	{
		ret.push_back(m_shaders[i]);
	}
	return ret;
}

void MultiShaderMaterial::set_shader(const Ref<Shader>& p_shader, int p_pos)
{
	int count = get_shader_count();
	if (count == 0)
	{
		insert_shader(p_shader, 0);
		return;
	}

	if (p_pos < 0)
	{
		p_pos = count + p_pos;
	}

	if (p_pos >= count || p_pos < 0)
		return;

	if (p_shader == m_shaders[p_pos])
		return;

	if (m_shaders[p_pos].is_valid() && g_shaderManager->has_shader_data(m_shaders[p_pos], m_shaderIds[p_pos]))
		g_shaderManager->unuse_shader(m_shaders[p_pos], m_shaderIds[p_pos], this);
	bool isUsing = m_shaders[p_pos].is_valid() && m_usingShaders.find(m_shaders[p_pos]);
	int max = -1;
	Set<int> frags;
	for (int i = 0; i < m_shaders.size(); ++i)
	{
		if (m_shaders[i] != p_shader)
			continue;

		if (max < m_shaderIds[i])
		{
			for (int j = max + 1; j < m_shaderIds[i]; ++j)
			{
				frags.insert(j);
			}
			max = m_shaderIds[i];
		}
		else
		{
			frags.erase(m_shaderIds[i]);
		}
	}
	m_shaders.ptrw()[p_pos] = p_shader;
	if (frags.size() > 0)
	{
		m_shaderIds.insert(p_pos, frags.front()->get());
	}
	else
	{
		m_shaderIds.insert(p_pos, max + 1);
	}

	if (!isUsing)
	{
		if (p_shader.is_null())
			return;

		if (m_usingShaders.size() > 0)
		{
			int pos = m_shaders.find(m_usingShaders.front()->get());
			if (p_pos > pos && p_shader->get_mode() != m_usingShaders.front()->get()->get_mode())
				return;
		}
	}
	_update();
}

Ref<Shader> MultiShaderMaterial::get_shader(int p_index) const
{
	int count = get_shader_count();
	if (p_index < 0)
	{
		p_index = count + p_index;
	}

	if (p_index >= count || p_index < 0)
		return Ref<Shader>();

	return m_shaders[p_index];
}

int MultiShaderMaterial::get_shader_count() const
{
	return m_shaders.size();
}

void MultiShaderMaterial::insert_shader(const Ref<Shader>& p_shader, int p_pos)
{
	int count = get_shader_count();
	if (p_pos < 0)
	{
		p_pos = count + p_pos + 1;
	}

	if (p_pos > count || p_pos < 0)
		return;

	int max = -1;
	Set<int> frags;
	for (int i = 0; i < m_shaders.size(); ++i)
	{
		if (m_shaders[i] != p_shader)
			continue;

		if (max < m_shaderIds[i])
		{
			for (int j = max + 1; j < m_shaderIds[i]; ++j)
			{
				frags.insert(j);
			}
			max = m_shaderIds[i];
		}
		else
		{
			frags.erase(m_shaderIds[i]);
		}
	}
	m_shaders.insert(p_pos, p_shader);
	if (frags.size() > 0)
	{
		m_shaderIds.insert(p_pos, frags.front()->get());
	}
	else
	{
		m_shaderIds.insert(p_pos, max + 1);
	}

	if (p_shader.is_null())
		return;

	if (m_usingShaders.size() > 0)
	{
		int pos = m_shaders.find(m_usingShaders.front()->get());
		if (p_pos > pos && p_shader->get_mode() != m_usingShaders.front()->get()->get_mode())
		{
			if (!p_shader->is_connected("changed", this, "_unusing_shader_changed"))
			{
				Ref<Shader> s = p_shader;
				s->disconnect("changed", this, "_unusing_shader_changed");
			}
			return;
		}
	}
	_update();
}

void MultiShaderMaterial::move_shader(int p_index, int p_pos)
{
	int count = get_shader_count();
	if (p_index < 0)
	{
		p_index = count + p_index;
	}
	if (p_index > count || p_index < 0)
		return;

	
	if (p_pos < 0)
	{
		p_pos = count + p_pos + 1;
	}
	if (p_pos > count || p_pos < 0)
		return;

	bool isUsing = m_shaders[p_pos].is_valid() && m_usingShaders.find(m_shaders[p_pos]);
	m_shaders.insert(p_pos, m_shaders[p_index]);
	m_shaderIds.insert(p_pos, m_shaderIds[p_index]);
	if (p_index > p_pos)
		p_index -= 1;
	m_shaders.remove(p_index);
	m_shaderIds.remove(p_index);

	if (isUsing)
	{
		_update();
	}
	else
	{
		_change_notify("shaders");
	}
}

void MultiShaderMaterial::clear_shader()
{
	_clear();
	if (m_usingShaders.size() > 0)
	{
		_update();
	}
	else {
		_change_notify("shaders");
	}
}

void MultiShaderMaterial::remove_shader(int p_index)
{
	int count = get_shader_count();
	if (p_index < 0)
	{
		p_index = count + p_index;
	}

	if (p_index >= count || p_index < 0)
		return;

	Ref<Shader> s = m_shaders[p_index];
	if (m_shaders[p_index].is_valid() && g_shaderManager->has_shader_data(m_shaders[p_index], m_shaderIds[p_index]))
		g_shaderManager->unuse_shader(m_shaders[p_index], m_shaderIds[p_index], this);
	m_shaders.remove(p_index);
	if (s.is_valid() && m_usingShaders.find(s))
	{
		_update();
	}
	else
	{
		if (!s->is_connected("changed", this, "_unusing_shader_changed") && m_shaders.find(s) < 0)
		{
			s->disconnect("changed", this, "_unusing_shader_changed");
		}
		_change_notify("shaders");
	}
}

void MultiShaderMaterial::set_shader_param(const StringName& p_param, const Variant& p_value, int p_index)
{
	if (m_usingShaders.size() <= 0)
		return;

	int count = get_shader_count();
	if (p_index < 0)
	{
		p_index = count + p_index;
	}
	if (p_index > count || p_index < 0 || m_shaders[p_index].is_null())
		return;

	if (!m_usingShaders.find(m_shaders[p_index]))
		return;

	if (m_usingShaders.size() == 1)
	{
		VS::get_singleton()->material_set_param(_get_material(), p_param, p_value);
	}
	else
	{
		String head = g_shaderManager->get_shader_key(m_shaders[p_index], m_shaderIds[p_index]);
		VS::get_singleton()->material_set_param(_get_material(), head + p_param, p_value);
	}
	
	_change_notify();
}

Variant MultiShaderMaterial::get_shader_param(const StringName& p_param, int p_index) const
{
	if (m_usingShaders.size() <= 0)
		return Variant();

	int count = get_shader_count();
	if (p_index < 0)
	{
		p_index = count + p_index;
	}
	if (p_index > count || p_index < 0 || m_shaders[p_index].is_null())
		return Variant();

	bool find = false;
	for (auto e = m_usingShaders.front(); e; e = e->next())
	{
		if (e->get() == m_shaders[p_index])
		{
			find = true;
			break;
		}
	}
	if (!find)
		return Variant();


	if (m_usingShaders.size() == 1)
		return VS::get_singleton()->material_get_param(_get_material(), p_param);

	String head = g_shaderManager->get_shader_key(m_shaders[p_index], m_shaderIds[p_index]);
	return VS::get_singleton()->material_get_param(_get_material(), head + p_param);
}

MultiShaderMaterial::MultiShaderMaterial()
{
}

MultiShaderMaterial::~MultiShaderMaterial()
{
	_clear();
}
