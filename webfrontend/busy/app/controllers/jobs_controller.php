<?php
class JobsController extends AppController {
	var $name = 'Jobs';

	function logfile($id){
		$job = $this->Job->read(null, $id);
		$logfile = file_get_contents($job['Backup']['path'] . DS . $job['Backup']['name'] . DS . $job['Job']['destdir'] . DS . 'rsync.log');
		$this->set('logfile', $logfile);
	}
}
?>
