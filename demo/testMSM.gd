extends Control

func get_shaders_hbox():
	return $VBoxContainer/ScrollContainer/Panel/shadersHbox

func get_test_nodes():
	return [$VBoxContainer/HBoxContainer/VBoxContainer/Control/Button,
			$VBoxContainer/HBoxContainer/VBoxContainer2/img,
			$VBoxContainer/HBoxContainer/VBoxContainer3/Control/Sprite]

func _ready():
	var nodes = get_test_nodes()
	var shadersHbox = get_shaders_hbox()
	for i in nodes:
		i.material = shadersHbox.testMaterial
