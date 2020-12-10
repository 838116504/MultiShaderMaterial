extends CheckBox

func create():
	var node = load("res://shaderHbox.tscn").instance()
	node.set_shader_name("3D着色器")
	node.set_shader(load("res://3D.shader"))
	return node
