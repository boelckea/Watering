<html>
 <head>
  <title>Log Info</title>
 </head>
 <body> 
 <?php
	$myfile = fopen("info.txt", "r") or die("Unable to open file!");
	while(!feof($myfile)) {
		$line = fgets($myfile);
		echo $line . "<br>\n";
	}
	fclose($myfile);
 ?>

 </body>
</html>