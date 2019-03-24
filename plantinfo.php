<!DOCTYPE HTML>
<html>
<head>
    <script type="text/javascript">
	<?php
		function IsNullOrEmptyString($str){
			return (!isset($str) || trim($str) === '');
		}
	?>
	
	<?php 
		$count = htmlspecialchars($_GET["count"]);
		if(IsNullOrEmptyString($count)){
			$count = 50;
		}
	
		$file = fopen("info.txt", "r") or die("Unable to open file!");
				
		$linecount = 0;
		while (!feof($file)) {
			$linecount += substr_count(fread($file, 8192), "\n");
		}

		for ($plantId = 1; $plantId <= 9; $plantId++) {
			rewind($file);
			$linenumber = 0;
			echo "var data".$plantId." = [";
			while(!feof($file)) {
				$line = fgets($file);
				if($linenumber >= ($linecount - $count)){
					$spli = preg_split("/,/",$line);
					if(!IsNullOrEmptyString($spli[0])){
					  echo "{color: '#".($plantId == 9 ? "4a8aff" : ($spli[$plantId] < 0 ? "da6a6a" : "04aa20"))."'," .
					  " x: new Date(" . substr($spli[0],0,4) .", " . (string)(intval(substr($spli[0],4,2)) - 1) .", " . substr($spli[0],6,2) .", " . substr($spli[0],8,2) .", " . substr($spli[0],10,2) .", " . substr($spli[0],12,2) ."),".
					  " y: " . ($plantId == 9 ? $spli[$plantId] :abs($spli[$plantId])) . " },";
					}
				}
				$linenumber++;
			}
			echo "];\n";
		} 
		fclose($file);
	?>
    </script>
    <script type="text/javascript" src="chart.min.js"></script>
</head>
<body>
	<?php 
		echo "Version: 13 linecount: " . $linecount;
	?>
	<a href="?count=<?php echo $count / 10 ?>">[count / 10]</a> -
	<a href="?count=<?php echo $count / 2 ?>">[count / 2]</a> -
	<a href="?count=<?php echo $count - 1 ?>">[count - 1]</a> -
	<a href="?count=<?php echo $count + 1 ?>">[count + 1]</a> -
	<a href="?count=<?php echo $count * 2 ?>">[count * 2]</a> -
	<a href="?count=<?php echo $count * 10 ?>">[count * 10]</a> 
	<?php 
		for ($plantId = 1; $plantId <= 9; $plantId++) {
			echo "<div id='chart".$plantId."' style='height: 150px; width: 100%;'></div>";
		}
		echo "<script>";
			echo "window.onload = function() {";
				for ($plantId = 1; $plantId <= 9; $plantId++) {
					echo "var chart".$plantId." = new CanvasJS.Chart('chart".$plantId."',{axisX: {valueFormatString: 'MMM D HH:mm'}, axisY: {title: '" . ($plantId == 9 ? "Tank" : "P" . $plantId) . "', maximum: " . ($plantId == 9 ? "800" : "1000") . "}, dataPointMinWidth: ".max(500/$count,1).", data: [{ xValueFormatString: 'YYYY.MM.DD HH:mm:ss', type: '" . ($plantId == 9 ? "area" : "column") . "',dataPoints: data".$plantId."}]});";
					echo "chart".$plantId.".render();";
				}
			echo "}";
		echo "</script>";
	?>
</body>
</html>