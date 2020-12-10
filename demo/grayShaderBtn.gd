extends CheckBox

func create():
	var node = load("res://shaderHbox.tscn").instance()
	node.set_shader_name("灰度着色器")
	node.set_shader(load("res://gray.shader"))
	return node
