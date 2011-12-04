<?php
	$jobsdiv = "";
	$cssClass = join(' ', array(
		$backup['Backup']['state'],
		!empty($backup['Backup']['exists']) ? 'exists' : 'corrupt'
	));
	foreach ($backup['Job'] as $job){
		$jobsdiv .= $html->tag('li', join(', ', array($job['srcdir'], $job['destdir'], $job['state'], $html->link('Rsync Logfile', '/jobs/logfile/'.$job['id'])/*, $job['rsync_cmd'], $job['started'], $job['finished']*/)));
	}
	echo $html->tableCells(array(
		"{$backup['Backup']['host_hostname']} ({$backup['Backup']['host_ip']})",
		strftime("%x %H:%M:%S", strtotime($backup['Backup']['started'])),
		strftime("%x %H:%M:%S", strtotime($backup['Backup']['finished'])),
		sprintf("%02u:%02u:%02u",
			$backup['Backup']['duration'] / 3600,
			$backup['Backup']['duration'] % 3600 / 60,
			$backup['Backup']['duration'] % 60
		),
		$this->Number->toReadableSize($backup['Backup']['size']),
		$backup['Backup']['type'],
		$backup['Backup']['state'],
		$html->link('Logfile', array('controller' => 'backups', 'action' => 'show_logfile', $backup['Backup']['id'])),
		$html->tag('ul', $jobsdiv)
	), array('class' => 'odd ' . $cssClass), array('class' => $cssClass));
?>
