<?php
class Job extends AppModel {
	var $name = 'Job';

	var $belongsTo = array('Backup');
	var $order = array('Job.finished' => 'DESC');
}
?>
