<?php
function HumanReadableFilesize($size) {
 
    // Adapted from: http://www.php.net/manual/en/function.filesize.php
 
    $mod = 1024;
 
    $units = explode(' ','B KB MB GB TB PB');
    for ($i = 0; $size > $mod; $i++) {
        $size /= $mod;
    }
 
    return round($size, 2) . ' ' . $units[$i];
}
?>

<h1>Hosts</h1>
<p><?php echo count($hosts); ?> configured Hosts</p>

<table>
	<thead>
		<?php echo $this->Html->tableHeaders(array(
			__('Name', true),
			__('Hostname', true),
			__('max_incr', true),
			__('max_age_full', true),
			__('max_age_incr', true),
			__('Backup dir', true),
			__('Actions', true)
		));
	?>
	</thead>
	<tbody>
		<?php
			foreach ($hosts as $host){
				echo $this->Html->tableCells(array(
					$this->Html->link($host['Host']['name'], array('controller' => 'hosts', 'action' => 'view', $host['Host']['id'])),
					$host['Host']['hostname'],
					$host['Host']['max_incr'],
					$host['Host']['max_age_full'],
					$host['Host']['max_age_incr'],
					$host['Host']['backupdir'],
					$this->Html->tag('ul', join('', array(
						$this->Html->tag('li', $this->Html->link(__('Start manual full backup now', true), '/hosts/manual/full/' . $host['Host']['id'])),
						$this->Html->tag('li', $this->Html->link(__('Start manual incr backup now', true), '/hosts/manual/incr/' . $host['Host']['id']))
					)))
				));
			}
		?>
	</tbody>
</table>

<?php
	$nJobs = count($jobs);
	if ($nJobs > 0):
?>
<h1>Jobs</h1>
<p><?php echo $nJobs; ?> running Jobs</p>

<table>
	<thead>
		<?php echo $html->tableHeaders(array(
			__('Started', true),
			__('PID', true),
			__('Hostname', true),
			__('Source Dir', true),
			__('Rsync Command', true)
		));?>
	</thead>
	<tbody>
		<?php foreach ($jobs as $job){
			echo $html->tableCells(array(
				$job['Job']['started'],
				$job['Job']['pid'],
				$job['Backup']['host_hostname'],
				$job['Job']['srcdir'],
				$job['Job']['rsync_cmd']
			));
		}?>
	</tbody>
</table>
<?php endif ?>
				

<h1>Backups</h1>

<p><?php echo count($backups); ?> Backups</p>

<table border="1">
	<thead>
		<?php echo $html->tableHeaders(array(
			__('Hostname/ IP', true),
/*
			__('Queued', true),
*/
			__('Started', true),
			__('Finished', true),
			__('Duration', true),
			__('Size', true),
			__('Type', true),
			__('State', true),
			__('Log', true),
			__('Jobs', true)
		));
	?>
	</thead>
	<tbody>
		<?php
		foreach ($backups as $backup){
			echo $this->element('backup', array('backup' => $backup));
		}
		?>
	</tbody>
</table>

