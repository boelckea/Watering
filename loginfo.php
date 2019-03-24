<html>
 <head>
  <title>Log Info</title>
 </head>
 <body> 
 <?php
	echo 'Info: ' . htmlspecialchars($_GET["info"]);
 ?>
 <br>
 <?php
	function IsNullOrEmptyString($str){
		return (!isset($str) || trim($str) === '');
	}
	
	$info = htmlspecialchars($_GET["info"]);
	if(!IsNullOrEmptyString($info)){
		$info = date('YmdHis') . "," . $info . "\n";
		file_put_contents('info.txt', $info, FILE_APPEND | LOCK_EX);
	}
 ?>
 <br>
 <?php
	//$myfile = fopen("info.txt", "r") or die("Unable to open file!");
	//while(!feof($myfile)) {
	//	$line = fgets($myfile);
	//	echo $line . "<br>\n";
	//}
	//fclose($myfile);
 ?>

 </body>
</html>