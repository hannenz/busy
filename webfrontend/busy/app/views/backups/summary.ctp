
<h1>Summary</h1>
<h2>Backups</h2>
<p>
There are <?php echo $total; ?> backups in total.
</p>
<h2>Hosts</h2>
<table>
	<thead>
		<?php echo $html->tableHeaders(array(
			__('Name', true),
			__('Hostname', true),
			__('IP', true),
			__('Backups', true),
			__('Last backup', true)
		)); ?>
	</thead>
	<tbody>
		<?php
			foreach ($hosts as $host){
				echo $html->tableCells(array(
					$html->link($host['Host']['name'], '/backups/index/'.$host['Host']['name']),
					$host['Host']['hostname'],
					$host['Host']['ip'],
					count($host['Backup']),
					$time->timeAgoInWords($host['Backup'][0]['Backup']['finished'])
				));
			}
		?>
	</tbody>
</table>
