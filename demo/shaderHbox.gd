extends HBoxContainer

var shader:Shader = null
var targetMaterial:MultiShaderMaterial = null
var shaderParams := []

class ShaderParam:
	var name:String
	var showName:String
	var minValue:float
	var maxValue:float
	var step:float
	var defaultValue:float
	var allowLesser
	var allowGreater
	var node:Node
	func create_node(p_connectTarget, p_connectFunc:String):
		if node:
			node.queue_free()
		node = HBoxContainer.new()
		var label = Label.new()
		label.text = showName
		label.mouse_filter = Control.MOUSE_FILTER_PASS
		node.add_child(label)
		var sb = SpinBox.new()
		sb.min_value = minValue
		sb.max_value = maxValue
		sb.allow_lesser = allowLesser
		sb.allow_greater = allowGreater
		sb.step = step
		sb.value = defaultValue
		sb.connect("value_changed", p_connectTarget, p_connectFunc, [self])
		sb.mouse_filter = Control.MOUSE_FILTER_PASS
		node.add_child(sb)
		return node
	
	func get_value():
		if !node:
			return null
		
		return node.get_child(1).value

func add_float_param(p_name:String, p_showName:String, p_min:float,p_max:float, p_step:float, p_default := 0.0, p_lesser := false, p_greater := false):
	var sp = ShaderParam.new()
	sp.name = p_name
	sp.showName = p_showName
	sp.minValue = p_min
	sp.maxValue = p_max
	sp.step = p_step
	sp.defaultValue = p_default
	sp.allowLesser = p_lesser
	sp.allowGreater = p_greater
	get_vbox().add_child(sp.create_node(self, "_on_param_value_changed"))
	shaderParams.push_back(sp)

func get_check_box():
	return $vbox/CheckBox

func get_vbox():
	return $vbox

func get_swap_btn():
	return $swapBtn

func is_enabled():
	return get_check_box().pressed

func is_valid():
	return is_enabled() && shader && targetMaterial

func get_pos() -> int:
	if !is_inside_tree():
		return -1
	
	var p = get_parent()
	var count = 0
	for i in p.get_children():
		if i == self:
			break;
		if i.has_method("is_valid") && i.is_valid():
			count += 1
	
	return count

func set_shader_name(p_name:String):
	get_check_box().text = p_name

func set_shader(p_shader:Shader):
	shader = p_shader

func _ready():
	get_check_box().connect("toggled", self, "_on_checkBox_toggled")
	get_swap_btn().connect("pressed", self, "_on_swapBtn_pressed")
	
	if is_valid():
		var pos = get_pos()
		targetMaterial.insert(shader, pos)
		for i in shaderParams:
			targetMaterial.set_shader_param(i.name, i.get_value(), pos)
	
	if get_index() > 0:
		get_swap_btn().show()
	else:
		get_swap_btn().hide()

func _exit_tree():
	get_check_box().disconnect("toggled", self, "_on_checkBox_toggled")
	get_swap_btn().disconnect("pressed", self, "_on_swapBtn_pressed")
	if is_valid():
		targetMaterial.remove(get_pos())

func set_target_material(p_material:MultiShaderMaterial):
	if is_inside_tree() && is_valid():
		targetMaterial.remove(get_pos());
	targetMaterial = p_material
	if is_inside_tree() && is_valid():
		targetMaterial.insert(shader, get_pos())

func _on_checkBox_toggled(p_pressed):
	if !is_inside_tree() || !targetMaterial || !shader:
		return
	
	if p_pressed:
		targetMaterial.insert(shader, get_pos())
	else:
		targetMaterial.remove(get_pos())

func _on_param_value_changed(p_value, p_param:ShaderParam):
	if !is_inside_tree() || !is_valid():
		return
	
	targetMaterial.set_shader_param(p_param.name, p_value, get_pos())

func _on_swapBtn_pressed():
	if !is_inside_tree() || get_index() == 0:
		return
	
	var id = get_index()
	var pos
	if (is_valid()):
		pos = get_pos()
	var swapTarget = get_parent().get_child(id - 1)
	get_parent().move_child(self, id - 1)
	if (id == 1):
		get_swap_btn().hide()
		swapTarget.get_swap_btn().show()
	
	if (is_valid() && swapTarget.is_valid()):
		targetMaterial.move(pos, get_pos())
	
