extends Button

func create():
	var node = load("res://shaderHbox.tscn").instance()
	node.set_shader_name("縮放着色器")
	node.set_shader(load("res://scale.shader"))
	node.add_float_param("scaleRatio", "比例", 0.0, 2.0, 0.01, 0.8, false, true)
	return node
