<?php
  $version_h_file = false;
  $dirs_to_scan = Array();

  $pattern = "\\.(c|cpp|h|inc|asm|s)$";

  if ($_SERVER['argc'] <= 1) {
    print(
"Usage:
php -f " . $_SERVER['argv'][0] . " [\"pattern=<pattern>\"] <version.h> <dir1> [<dir2> [<dir3>...]]
<pattern> - a regular expression pattern to match filenames. Default is $pattern
<dir...> - directories to look for source files.
<version.h> - full path to the file with macro defintions.

If one of matching files in specified directories has the modification date greater than the modification date of the <version.h> file, then <version.h> is processed and macros, defined using #define directive followed by the macro name and a numerical value, are processed as follows:
    BUILD_YEAR, BUILD_MONTH, BUILD_DAY, BUILD_HOUR, BUILD_MINUTE, BUILD_SECOND - updates to the respective value of the maximal modification date of files matched to pattern in specified directories
    BUILD_NUMBER - automatically increases by one.
");
    die();
  }

  foreach ($_SERVER['argv'] as $p => $v) {
    if ($p > 0) {
      if (preg_match("/(^[A-Za-z]+)=(.*)$/", $v, $r)) {
        $param = $r[1];
        $val = $r[2];
        if ($param == 'pattern') {
          $pattern = $val;
          if (strpos($pattern, '/') !== false) die('ERROR: pattern should not contain the / (slash) symbol');
        } else {
          die('ERROR: Unknown parameter ' . $param . "\r\n");
        }
      } elseif ($version_h_file === false) {
        if (!is_file($v)) die('ERROR: file ' . $v . " does not exist!\r\n");
        $version_h_file = $v;
      } else {
        if (!is_dir($v)) die('ERROR: directory ' . $v . " does not exist!\r\n");
        $dirs_to_scan[] = $v;
      }
    }
  }
  if ($version_h_file === false) die("ERROR: The version header file is not specified\r\n");

  if (count($dirs_to_scan) == 0) {
    $default_dir = pathinfo($version_h_file, PATHINFO_DIRNAME);
    print('Warning: No one directory to scan is specified. Default used: ' . $default_dir . "\r\n");
    $dirs_to_scan[] = $default_dir;
  }

  $regexp = '/' . $pattern . '/';
  $maxtime = 0;
  $maxtimefn = false;
  foreach($dirs_to_scan as $dir) {
     if (($dh = opendir ($dir))) {
        while (($file = readdir($dh)) !== false) {
          $fn = $dir . '/' . $file;
          if (is_file($fn) && preg_match($regexp, $file)) {
            $tm = filemtime($fn);
            if ($tm > $maxtime) {
              $maxtime = $tm;
              $maxtimefn = $fn;
            }
          }
        }
        closedir($dh);
     }
  }

  if ($maxtime <= filemtime($version_h_file)) die("No new changes in the source files\r\n");

  print("Increasing version number. Last modified " . date('j.n.Y H:i:s', $maxtime) . " file " . $maxtimefn . "\r\n");

  $temp_file = preg_replace('#^(\S+?)(\.([^\.\\/]*))?$#', '\\1.~\\3', $version_h_file);

  $f = fopen($version_h_file, 'r') or die('ERROR: cannot open ' . $version_h_file);
  if (!($fout = fopen($temp_file, 'w'))) {
    fclose($f);
    die('ERROR: cannot write ' . $temp_file );
  }

  $changed = false;

  while (!feof($f)) {
    $s = fgets($f, 1024);

    if (preg_match('/^\s*#define\s+(\S+)\s+(\d+)/', $s, $r, PREG_OFFSET_CAPTURE)) {
      $macros_name = $r[1][0];
      $value = $r[2][0];
      $value_pos = $r[2][1];

      $new_value = false;
      if ($macros_name == 'BUILD_YEAR') $new_value = date('Y', $maxtime);
      elseif ($macros_name == 'BUILD_MONTH') $new_value = date('n', $maxtime);
      elseif ($macros_name == 'BUILD_DAY') $new_value = date('j', $maxtime);
      elseif ($macros_name == 'BUILD_HOUR') $new_value = date('G', $maxtime);
      elseif ($macros_name == 'BUILD_MINUTE') $new_value = intval(date('i', $maxtime));
      elseif ($macros_name == 'BUILD_SECOND') $new_value = intval(date('s', $maxtime));
      elseif ($macros_name == 'BUILD_NUMBER') {
        $new_value = intval($value) + 1;
        print("Build number increased to $new_value\r\n");
      }
      if ($new_value !== false) {
        $changed = true;
        $s = substr($s, 0, $value_pos) . $new_value . substr($s, $value_pos + strlen($value));
      }
    }
    fputs($fout, $s);
  }

  fclose($fout);
  fclose($f);

  if (!$changed) {
    unlink($temp_file) or die('ERROR: cannot delete ' . $temp_file);
    die('Warning: No one matched macro defintion is found in ' . $version_h_file . "\r\n");
  } else {
    unlink($version_h_file) or die('ERROR: cannot delete ' . $version_h_file . "\r\n");
    rename($temp_file, $version_h_file) or die('ERROR: cannot rename ' . $temp_file . ' to ' . $version_h_file . "\r\n");
  }

  print("Build version update complete\r\n");


