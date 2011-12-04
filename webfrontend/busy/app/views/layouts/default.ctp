<!DOCTYPE html>
<html>
	<head>
		<title>BUSY - Backup System</title>
		<?php
			echo $html->charset('utf-8');
			echo $html->script(array(
			));
			echo $html->css(array(
				'main.css'
			));
			echo $scripts_for_layout;
		?>
	</head>
	<body>
		<div id="header">
			<h1>Busy</h1>
			<h2>BackUp SYstem</h2>
		</div>
		
		<?php
			echo $this->Session->flash();
			echo $content_for_layout;
		?>
	</body>
</html>
