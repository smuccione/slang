<?php

  import_request_variables('g', 'p_');
$adminpass = "test123";
if ($p_pw == $adminpass)
{
   print("Welcome to the administration area!");
}
  else
{
   print("Wrong password2 " . '-'. $p_pw . '-'.$_GET["pw"] );
}
?>