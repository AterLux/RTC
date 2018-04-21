<?php

  if ($_SERVER['argc'] < 3) die("ERROR: Use: php -f " . $_SERVER['argv'][0] . " <source_name> <dest_c_name>\r\n");
  $src_file = $_SERVER['argv'][1];
  $dest_c_file = $_SERVER['argv'][2];

  if (!file_exists($src_file)) die('ERROR: File ' . $src_file . " does not exist!\r\n");

  // Если менялся сам скрипт, то учтём это тоже и перегенерируем файл
  $mtime = max(filemtime($src_file), filemtime($_SERVER["SCRIPT_FILENAME"]));

  if (file_exists($dest_c_file) && (filemtime($dest_c_file) == $mtime)) {
    die($src_file . " was not modified\r\n");
  }

  $f = fopen($src_file, 'rb') or die("ERROR: cannot open file $src_file\r\n");

  print("Converting $src_file into $dest_c_file...\r\n");

  function closedie($message) {
    global $f;
    if ($f) fclose($f);
    return die($message);
  }

  $header_data = fread($f, 54);
  if (strlen($header_data) < 54) die("ERROR: Error reading the bitmap file header\r\n");

  $header = unpack('vtype/Vsize/Vreserved/Voffbits/Vinfosize/Vwidth/Vheight/vplanes/vbitcount/Vcompression/Vsizeimage/Vxppm/Vyppm/Vcirused/Vcirimportant', $header_data);

  if ($header['type'] != 0x4D42) closedie("ERROR: Unsupported file format\r\n");
  if ($header['infosize'] < 40) closedie("ERROR: Unsupported header type\r\n");
  if ($header['bitcount'] != 24) closedie("ERROR: Unsupported pixel format (24 bits per pixel expected)\r\n");
  if ($header['compression'] != 0) closedie("ERROR: Unsupported compression type (plain RGB expected)\r\n");

  $width = $header['width'];
  $height = $header['height'];

  if (($width < 16) || ($height < 16) || ($width > 4096) || ($height > 1024)) closedie('ERROR: Unsupported bitmap size: ' . $width . ' x ' . $height . "\r\n");


  $bits_offset = $header['offbits'];

  $bytes_in_row = ($width * 3) + ($width % 4); // В bmp строки выровнены кратно 4 байтам

  $pix_count = (int)floor($width / 16);
  $active_width = $pix_count * 16;


  $pix_data = array();

  $pixels = array(); // Массив строк, где каждый символ соответствуют 1 пикселю, и принимает значение 0 или 1

  for ($y = 0 ; $y < 16; $y++) {
    fseek($f, $bits_offset + (($height - $y - 1) * $bytes_in_row));
    $row_data = fread($f, $width * 3);

    $pix_line = '';
    for ($x = 0 ; $x < $active_width ; $x++) {
      if ($x > $width) {
        $pix_line .= '0';
      } else {
        $i = $x * 3;
        $pix_line .= ((ord($row_data[$i]) + ord($row_data[$i + 1]) * 2 + ord($row_data[$i + 2])) >= 512) ? '1' : '0';
      }
    }
    $pixels[$y] = $pix_line;
  }
  fclose($f);

  $text = "PROGMEM uint8_t const pix_data[][2][16] = {\r\n  ";
  for ($pixn = 0 ; $pixn < $pix_count ; $pixn++) {
    $pixx = $pixn * 16;

    $line = '';
    for ($ypg = 0 ; $ypg < 16 ; $ypg += 8) {
      if ($ypg > 0) $line .= ",\r\n";
      $line .= '    { ';
      for ($cx = 0 ; $cx < 16 ; $cx++) {
        $x = $pixx + $cx;
        $d = 0;
        for ($y = 0 ; $y < 8 ; $y++) {
          if ($pixels[$ypg + $y][$x] == '1')
            $d |= (1 << $y);
        }
        if ($cx > 0) $line .= ', ';
        $line .= '0x' . str_pad(strtoupper(dechex($d)), 2, '0', STR_PAD_LEFT);
      }
      $line .= ' }';
    }
    if ($pixn > 0) $text .= ", ";
    $text .= "{ // $pixn\r\n" . $line . "\r\n  }";
  }
  $text .= "\r\n};\r\n";

  print("Generating source for $pix_count images...\r\n");


  $fout = fopen($dest_c_file, 'wb') or die('ERROR: cannot write file ' . $dest_c_file);

  fputs($fout, 
"// This file is generated automatically
// Source file timestamp " . date('j.n.Y H:i:s', $mtime) ."
// Generated at " . date('j.n.Y H:i:s') . "
// $pix_count images 

#include <avr/io.h>
#include <avr/pgmspace.h>

$text
");

  fclose($fout);
  touch($dest_c_file, $mtime);

  print("Pix sources have been successfuly generated\r\n");
?>