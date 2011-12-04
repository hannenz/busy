<?php
class HostsController extends AppController {
	var $name = 'Hosts';
	var $uses = array('Host', 'Backup');

	var $helpers = array('Number');
	
	function manual($type, $id){
	
		$host = $this->Host->read(null, $id);
		if ($type == "full" || $type == "incr"){
			$cmd = "echo \"backup $type {$host['Host']['name']}\" | nc -q 1 localhost 4000";
			exec ($cmd);
			$this->Session->setFlash('Command has been sent to Daemon: ' . $cmd);
		}
		else {
			$this->Session->setFlash('Invalid backup type: ' . $type);
		}
		$this->redirect($this->referer());
	}
	
	function view($id){
		$host = $this->Host->read(null, $id);
		
		$backups = $this->Backup->find('all', array(
			'conditions' => array(
				'Backup.host_hostname' => $host['Host']['hostname']
			)
		));
		foreach ($backups as $backup){
			$host['Backup'][] = $backup;
		}

		$this->set('host', $host);
	}
}
