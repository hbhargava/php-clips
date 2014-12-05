<?php
if(!function_exists('get_default')) {
	function get_default($arr, $key, $default = null) {
		if(isset($arr)) {
			$a = (array) $arr;
			return isset($a[$key])? $a[$key]: $default;
		}
		return null;
	}
}

function clips_get_property($obj, $property) {
	if(is_array($obj) && isset($obj[$property])) {
		return $obj[$property];
	}

	if(is_object($obj) && isset($obj->$property)) {
		return $obj->$property;
	}
	return null;
}
/**
 *  The clips extension engine and execution context
 */
class Clips {

	/**
	 * The clips execution context
	 */
	public static $context;

	public function __construct() {
		if(!Clips::$context) {
			Clips::$context = array();
			clips_init(Clips::$context);
			$this->defineClasses();
			$this->defineMethods();
		}
	}

	private function defineMethods() {
		$this->command('(defmethod php_property ((?obj INSTANCE-NAME INSTANCE-ADDRESS) (?property STRING)) (php_call "clips_get_property" ?obj ?property))'); // Define the php_property function
	}

	private function defineClasses() {
		if(!$this->classExists('PHP_OBJECT'))
			$this->command($this->defineClass('PHP_OBJECT', 
				array('USER', 'OBJECT'), false, array()));
	}

	private function translate($var) {
		switch(gettype($var)) {
		case 'string':
			return '"'.$var.'"';
		case 'array':
		case 'object':
			// For array and object, let's make them multiple values
			$ret = array();
			foreach($var as $key => $value) {
				$ret []= $this->translate($value);
			}
			return implode(' ', $ret);
		}
		return $var;
	}

	public function defineSlot($name, $type = 'slot', $default = null, $constraints = array()) {
		$slot = array();
		$slot []= '('.$type; // Add the slot define
		$slot []= $name;
		if($default !== null) {
			$slot []= '(default '.$default.')';
		}

		foreach($constraints as $c) {
			if(isset($c['type'])) {
				switch($c['type']) {
				case 'range':
				case 'cardinality': // For range and cardinality, we use 2 parameters
					$slot []= '('.$c['type'].' '.$c['begin'].' '.$c['end'].')'; // Default is (type value)
					break;
				default:
					$slot []= '('.$c['type'].' '.$c['value'].')'; // Default is (type value)
				}
			}
		}
		return implode(' ', $slot).')';
	}

	public function defineClass($class, $parents, $abstract = false, $slots = null, $methods = null) {
		$ret = array();
		$ret []= '(defclass '.$class;
		$p = $parents;
		if(is_array($parents)) {
			$p = implode(' ', $parents);
		}

		$ret []= '(is-a '.$p.')';

		if($abstract) {
			$ret []= '(role abstract)';
		}
		else {
			$ret []= '(role concrete)';
		}

		if($slots != null) {
			foreach($slots as $slot) {
				if(is_string($slot))
					$ret []= $this->defineSlot($slot);
				else
					$ret []= $this->defineSlot(
						$slot['name'], 
						get_default($slot, 'type', 'slot'),
						get_default($slot, 'default'),
						get_default($slot, 'constraints', array())
					);
			}
		}

		return implode(' ', $ret).')';
	}

	public function reset() {
		$this->command('(reset)');
	}

	public function clear() {
		$this->command('(clear)');
	}

	public function facts() {
		$this->command('(facts)');
	}

	public function run() {
		$this->command('(run)');
	}

	public function __get($key) {
		if(isset(Clips::$context[$key])) {
			return Clips::$context[$key];
		}
		return $this->$key;
	}

	public function defineInstance($name, $class = 'PHP_OBJECT', $args = array()) {
		$ret = array();
		$ret []= '(make-instance';
		$ret []= $name;
		$ret []= 'of';
		$ret []= $class;
		foreach($args as $key => $value) {
			$ret []= '('.$key;
			$ret []= $value;
			$ret []= ')';
		}
		return implode(' ', $ret).')';
	}

	public function __set($key, $value) {
		Clips::$context[$key] = $value;
		if(!$this->instanceExists($key)) {
			$this->command($this->defineInstance($key));
		}
	}

	/**
	 * Start the clips in console mode
	 */
	public function console() {
		$line = readline('pclips$ ');
		$line .= "\n";
		while(true) {
			if(clips_is_command_complete($line)) {
				$this->command($line);
				readline_add_history($line);
				$line = readline('pclips$ ');
			}
			else {
				$line .= readline('... ');
			}
		}
	}

	public function instanceExists($name) {
		if($name)
			return clips_instance_exists($name);
		return false;
	}

	public function classExists($class) {
		if($class)
			return clips_class_exists($class);
		return false;
	}

	public function templateExists($template) {
		if($template)
			return clips_template_exists($template);
		return false;
	}

	public function assertFact($data) {
		$ret = array();
		if(is_array($data))  {
			if(!isset($data['template'])) { // This is a static fact
				$name = array_shift($data);
				$ret []= '('.$name;
				foreach($data as $d) {
					$ret []= $this->translate($d);
				}
			}
			else {
				return $this->assertFact((object) $data);
			}
		}
		else {
			$obj = $data;
			$name = get_class($obj);
			if(isset($obj->template)) {
				$name = $obj->template;
			}

			$ret []= '('.$name;
			foreach($obj as $key => $value) {
				if($key == 'template') // Skip template
					continue;
				$ret []= '('.$key;
				$ret []= $this->translate($value).')';
			}
		}
		return implode(' ', $ret).')';
	}

	/**
	 * Execute the clips command
	 */
	public function command($command) {
		if(is_array($command)) {
			foreach($command as $c) {
				$this->command($c);
			}
			return;
		}
		clips_exec($command."\n"); // Add \n automaticly
	}

	/**
	 * Load and execute the clips rule file
	 */
	public function load($file) {
		if(is_array($file)) {
			foreach($file as $f) {
				$this->load($f);
			}
			return;
		}

		if(file_exists($file)) {
			clips_load($file);
		}
		else {
			trigger_error('The file '.$file.' is not found.');
		}
	}
}