<html>
 <head>
  <title>Plantinfo Settings</title>
 </head>
 <body> 
 <?php
	echo "Settings.txt:<br>\n";
	$myfile = fopen("settings.txt", "r") or die("Unable to open file!");
	while(!feof($myfile)) {
		$line = fgets($myfile);
		echo $line . "<br>\n";
	}
	fclose($myfile);
 ?>
 <?php
	echo 'Settings arg: ' . htmlspecialchars($_GET["settings"]);
 ?>
 <br>
 <?php
	function IsNullOrEmptyString($str){
		return (!isset($str) || trim($str) === '');
	}
	
	$info = htmlspecialchars($_GET["settings"]);
	if(!IsNullOrEmptyString($info)){
		file_put_contents('settings.txt', $info, LOCK_EX);
	}
 ?>
 <br>
 </body>
</html>