<?php
class Backup extends AppModel {
	var $name = 'Backup';

	var $hasMany = array('Job');
	var $order = array('Backup.finished' => 'DESC');

	function afterFind($results, $primary = true){
	
		foreach ($results as $key => $result){
			if ($primary){
				$results[$key]['Backup']['exists'] = file_exists($result['Backup']['path'] . DS . $result['Backup']['name']);
			}
		}
		return ($results);
	}

}
?>
