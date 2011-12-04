<?php
class BackupsController extends AppController {
	var $name = 'Backups';
	var $helpers = array('Time', 'Number');

	var $uses = array('Backup', 'Host');

	function index($host = null){
		if ($host){
			$backups = $this->Backup->findAllByHostName($host);
			$jobs = $this->Backup->Job->find('all', array('conditions' => array(
				'Job.state' => 'running',
				'Backup.host_name' => $host
			)));
		}
		else {
			$backups = $this->Backup->find('all');
			$jobs = $this->Backup->Job->find('all', array('conditions' => array(
				'Job.state' => 'running'
			)));
				
		}
		
		foreach ($backups as $key => $backup){
			$backups[$key]['Backup']['exists'] = file_exists($backup['Backup']['path'] . DS . $backup['Backup']['name']);
		}
	
		
		$hosts = $this->Host->find('all');
		$this->set('hosts', $hosts);
		$this->set('backups', $backups);
		$this->set('jobs', $jobs);
	}

	function get_all_hosts(){
		$backups = $this->Backup->find('all');
		$hosts = array();
		$h = array();
		foreach ($backups as $bu){
			if (!in_array($bu['Backup']['host_name'], $h)){
				$h[] = $bu['Backup']['host_name'];
				$hosts[] = array(
					'Host' => array(
						'name' => $bu['Backup']['host_name'],
						'hostname' => $bu['Backup']['host_hostname'],
						'ip' => $bu['Backup']['host_ip'],
					),
					'Backup' => $this->Backup->find('all', array('conditions' => array('host_name' => $bu['Backup']['host_name'])))
				);
			}
		}
		return ($hosts);
	}

	function summary(){
		$hosts = $this->get_all_hosts();
		$total = $this->Backup->find('count');
		$this->set('hosts', $hosts);
		$this->set('total', $total);
	}
	
	function show_logfile($id){
		$backup = $this->Backup->read(null, $id);
		$logfile = file_get_contents($backup['Backup']['path'] . DS . $backup['Backup']['name'] . DS . 'backup.log');
		$this->set('logfile', $logfile);
	}
}
?>
