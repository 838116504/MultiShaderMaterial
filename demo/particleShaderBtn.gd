extends CheckBox

func create():
	var node = load("res://shaderHbox.tscn").instance()
	node.set_shader_name("粒子着色器")
	node.set_shader(load("res://particles.shader"))
	return node
