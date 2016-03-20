<?php
   	include("connect.php");
   	
	if ($_SERVER['REQUEST_METHOD'] != 'POST')
	{
   	  $link=Connection();
	  
	  $query = "INSERT INTO `acclog` (`SSID`) VALUES ('ERRPOST')";
	  
	  mysql_query($query,$link);
	  mysql_close($link);
	  return;
	}
	
   	$link=Connection();

	$factor = 1000.0;
	
	if (isset($_POST["tc1"]) && !empty($_POST["tc1"]))
		$tc1=$_POST["tc1"];
	else $tc1=0;
	if (isset($_POST["tm1"]) && !empty($_POST["tm1"]))
		$tm1=$_POST["tm1"];
	else $tm1=0;
	
	if (isset($_POST["tc2"]) && !empty($_POST["tc2"]))
		$tc2=$_POST["tc2"];
	else $tc2=0;
	if (isset($_POST["tm2"]) && !empty($_POST["tm2"]))
		$tm2=$_POST["tm2"];
	else $tm2=0;
	
	if (isset($_POST["acc1"]) && !empty($_POST["acc1"]))
		$raw0=$_POST["acc1"];
	else $raw0=0;

	if (isset($_POST["acc2"]) && !empty($_POST["acc2"]))
		$raw1=$_POST["acc2"];
	else $raw1=0;

	if (isset($_POST["acc3"]) && !empty($_POST["acc3"]))
		$raw2=$_POST["acc3"];
	else $raw2=0;

	if (isset($_POST["gyro1"]) && !empty($_POST["gyro1"]))
		$raw3=$_POST["gyro1"];
	else $raw3=0;

	if (isset($_POST["gyro2"]) && !empty($_POST["gyro2"]))
		$raw4=$_POST["gyro2"];
	else $raw4=0;

	if (isset($_POST["gyro3"]) && !empty($_POST["gyro3"]))
		$raw5=$_POST["gyro3"];
	else $raw5=0;

	if (isset($_POST["magn1"]) && !empty($_POST["magn1"]))
		$raw6=$_POST["magn1"];
	else $raw6=0;

	if (isset($_POST["magn2"]) && !empty($_POST["magn2"]))
		$raw7=$_POST["magn2"];
	else $raw7=0;

	if (isset($_POST["magn3"]) && !empty($_POST["magn3"]))
		$raw8=$_POST["magn3"];
	else $raw8=0;

	if (isset($_POST["temp1"]) && !empty($_POST["temp1"]))
		$raw9=$_POST["temp1"];
	else $raw9=0;

	if (isset($_POST["press1"]) && !empty($_POST["press1"]))
		$raw10=$_POST["press1"];
	else $raw10=0;
	
	if (isset($_POST["quat1"]) && !empty($_POST["quat1"]))
		$quat1=$_POST["quat1"]/$factor;
	else $quat1=0;
	
	if (isset($_POST["quat2"]) && !empty($_POST["quat2"]))
		$quat2=$_POST["quat2"]/$factor;
	else $quat2=0;
	
	if (isset($_POST["quat3"]) && !empty($_POST["quat3"]))
		$quat3=$_POST["quat3"]/$factor;
	else $quat3=0;
	
	if (isset($_POST["quat4"]) && !empty($_POST["quat4"]))
		$quat4=$_POST["quat4"]/$factor;
	else $quat4=0;
	
	if (isset($_POST["quat5"]) && !empty($_POST["quat5"]))
		$quat5=$_POST["quat5"]/$factor;
	else $quat5=0;
	
	if (isset($_POST["quat6"]) && !empty($_POST["quat6"]))
		$quat6=$_POST["quat6"]/$factor;
	else $quat6=0;
	
	if (isset($_POST["quat7"]) && !empty($_POST["quat7"]))
		$quat7=$_POST["quat7"]/$factor;
	else $quat7=0;
	
	if (isset($_POST["eurl1"]) && !empty($_POST["eurl1"]))
		$eurl1=$_POST["eurl1"]/$factor;
	else $eurl1=0;
	
	if (isset($_POST["eurl2"]) && !empty($_POST["eurl2"]))
		$eurl2=$_POST["eurl2"]/$factor;
	else $eurl2=0;
	
	if (isset($_POST["eurl3"]) && !empty($_POST["eurl3"]))
		$eurl3=$_POST["eurl3"]/$factor;
	else $eurl3=0;
	
	if (isset($_POST["yaw1"]) && !empty($_POST["yaw1"]))
		$yaw1=$_POST["yaw1"]/$factor;
	else $yaw1=0;
	
	if (isset($_POST["yaw2"]) && !empty($_POST["yaw2"]))
		$yaw2=$_POST["yaw2"]/$factor;
	else $yaw2=0;
	
	if (isset($_POST["yaw3"]) && !empty($_POST["yaw3"]))
		$yaw3=$_POST["yaw3"]/$factor;
	else $yaw3=0;


	if (isset($_POST["SSID"]) && !empty($_POST["SSID"]))
		$ssid=$_POST["SSID"];
	else $ssid="Unknown";

	if (isset($_POST["dBm"]) && !empty($_POST["dBm"]))
		$dbm=$_POST["dBm"];
	else $dbm=0;
	
	$query = "INSERT INTO `acclog` (`tc1`, `tm1`, `tc2`, `tm2`, `acc1`, `acc2`, `acc3`, `gyro1`, `gyro2`, `gyro3`, `magn1`, `magn2`, `magn3`, `temp1`, `press1`, `quat1`, `quat2`, `quat3`, `quat4`, `quat5`, `quat6`, `quat7`, `eurl1`, `eurl2`, `eurl3`, `yaw1`, `yaw2`, `yaw3`, `SSID`, `dBm`)
	        VALUES ($tc1,$tm1,$tc2,$tm2,$raw0,$raw1,$raw2,$raw3,$raw4,$raw5,$raw6,$raw7,$raw8,$raw9,$raw10,$quat1,$quat2,$quat3,$quat4,$quat5,$quat6,$quat7,$eurl1,$eurl2,$eurl3,$yaw1,$yaw2,$yaw3,'$ssid',$dbm)";
		 
   	mysql_query($query,$link);
	mysql_close($link);
	
?>
