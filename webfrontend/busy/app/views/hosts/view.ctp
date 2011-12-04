<?php
extract ($host);
?>

<h1><?php echo $Host['name']; ?></h1>

<dl>
	<dt>Name</dt>
	<dd><?php echo $Host['name']; ?></dd>
	<dt>Hostname</dt>
	<dd><?php echo $Host['hostname']; ?></dd>
	<dt>IPs</dt>
	<dd><?php echo $Host['ips']; ?></dd>
	<dt>Source directories</dt>
	<dd>
		<ul>
			<?php foreach (explode("\n", rtrim($Host['srcdirs'], "\n")) as $srcdir){
				echo $this->Html->tag('li', $srcdir);
			}
			?>
		</ul>
	<dt>Backup directory</dt>
	<dd><?php echo $Host['backupdir']; ?></dd>
	<dt>Schedule</dt>
	<dd>
		<ul>
			<?php foreach (explode("\n", rtrim($Host['schedule'], "\n")) as $sched){
				echo $this->Html->tag('li', $sched);
			}
			?>
		</ul>
	</dd>
	<dt>Excludes</dt>
	<dd>
		<ul>
			<?php foreach (explode("\n", rtrim($Host['excludes'], "\n")) as $exclude){
				echo $this->Html->tag('li', $exclude);
			}
			?>
		</ul>
	</dd>
	<dt>Max incremental backups</dt>
	<dd><?php echo $Host['max_incr']; ?></dd>
	<dt>Max age of incr. backup</dt>
	<dd><?php echo $Host['max_age_incr']; ?></dd>
	<dt>Max age of full backup</dt>
	<dd><?php echo $Host['max_age_full']; ?></dd>
</dl>

<h2>Backups for <?php echo $Host['hostname']; ?></h2>

<table>
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
			foreach ($host['Backup'] as $backup) {
				echo $this->element('backup', array('backup' => $backup));
			}
		?>
	</tbody>
</table>

<?php echo $this->Html->link(__('back', true), '/backups'); ?>
