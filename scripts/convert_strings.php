<?php

  if ($_SERVER['argc'] <= 3) {
    print(
"Usage:
php -f " . $_SERVER['argv'][0] . " <lang_dir> <c_file> <h_file> [<default_lang>]
<lang_dir> - the directory with language files. Each language file should be named as \"lang-<lang_code>.txt\"
<c_file> - full path to where generated c file should be stored
<h_file> - full path to where generated header file should be stored
<default_lang> - language code to be loaded as default in the generated source
");
    die();
  }

  $dir_lang = $_SERVER['argv'][1];
  $output_c_filename = $_SERVER['argv'][2];
  $output_h_filename = $_SERVER['argv'][3];
  $default_lang = isset($_SERVER['argv'][4]) ? strtolower($_SERVER['argv'][4]) : false;

  if (!is_dir($dir_lang)) die("ERROR: directory $dir_lang does not exist\r\n");

  $chars_map = Array(
    'А' => '\x80', 'Б' => '\x81', 'В' => '\x82', 'Г' => '\x83', 'Д' => '\x84', 'Е' => '\x85', 'Ж' => '\x86', 'З' => '\x87', 
    'И' => '\x88', 'Й' => '\x89', 'К' => '\x8A', 'Л' => '\x8B', 'М' => '\x8C', 'Н' => '\x8D', 'О' => '\x8E', 'П' => '\x8F', 
    'Р' => '\x90', 'С' => '\x91', 'Т' => '\x92', 'У' => '\x93', 'Ф' => '\x94', 'Х' => '\x95', 'Ц' => '\x96', 'Ч' => '\x97', 
    'Ш' => '\x98', 'Щ' => '\x99', 'Ъ' => '\x9A', 'Ы' => '\x9B', 'Ь' => '\x9C', 'Э' => '\x9D', 'Ю' => '\x9E', 'Я' => '\x9F',
    'а' => '\xA0', 'б' => '\xA1', 'в' => '\xA2', 'г' => '\xA3', 'д' => '\xA4', 'е' => '\xA5', 'ж' => '\xA6', 'з' => '\xA7', 
    'и' => '\xA8', 'й' => '\xA9', 'к' => '\xAA', 'л' => '\xAB', 'м' => '\xAC', 'н' => '\xAD', 'о' => '\xAE', 'п' => '\xAF', 
    'р' => '\xB0', 'с' => '\xB1', 'т' => '\xB2', 'у' => '\xB3', 'ф' => '\xB4', 'х' => '\xB5', 'ц' => '\xB6', 'ч' => '\xB7', 
    'ш' => '\xB8', 'щ' => '\xB9', 'ъ' => '\xBA', 'ы' => '\xBB', 'ь' => '\xBC', 'э' => '\xBD', 'ю' => '\xBE', 'я' => '\xBF',
    'Ё' => '\xC0', 'ё' => '\xC1', 'Ä' => '\xC2', 'ä' => '\xC3', 'Ї' => '\xC4', 'ї' => '\xC5', 'Ö' => '\xC6', 'ö' => '\xC7', 
    'Ü' => '\xC8', 'ü' => '\xC9', 'Ў' => '\xCA', 'ў' => '\xCB', 'Є' => '\xCC', 'є' => '\xCD', 'Ґ' => '\xCE', 'ґ' => '\xCF', 
    'ß' => '\xD0', '°' => '\xD1', '²' => '\xD2', '³' => '\xD3',
    

    // Синонимы
    'ẞ' => '\xD0'
  );

  $langs = Array();

  // Если менялся сам скрипт, или изменилась дата директории, то перегенерируем файлы
  $maxmtime = max(filemtime($_SERVER["SCRIPT_FILENAME"]), filemtime($dir_lang));

  if (!($dh = opendir ($dir_lang))) die("ERROR: Cannot open directory $dir_lang\r\n");
  while (($file = readdir($dh)) !== false) {
    $fn = $dir_lang . '/' . $file;
    if (is_file($fn) && preg_match('/lang-([a-zA-Z]+).txt/', $file, $r)) {
      $tm = filemtime($fn);
      if ($tm > $maxmtime) {
        $maxmtime = $tm;
      }
      $lang_code = strtolower($r[1]);
      $langs[$lang_code] = Array('filename' => $fn);
    }
  }
  closedir($dh);

  if (count($langs) == 0) die("ERROR: No lang files found (lang-<lang_code>.txt) in $dir_lang\r\n");

  if (file_exists($output_c_filename) && file_exists($output_h_filename) &&
      (filemtime($output_c_filename) == $maxmtime) && (filemtime($output_h_filename) == $maxmtime)) {
    die("No new changes in the language files\r\n");
  }


  foreach($langs as $lang_code => &$lang) {
    $fn = $lang['filename'];
    if (!$f = fopen($fn, 'r')) die("ERROR: Cannot open $fn\r\n");
    $str_num = 0;
    $lang['strings'] = Array();

    while (!feof($f)) {
      $s = trim(fgets($f, 1024));
      if (substr($s, 0, 3) == "\xEF\xBB\xBF") $s = substr($s, 3);
      $str_num++;
      if (preg_match('/^\s*([A-Za-z]+)\s*:\s*+(.*+)\s*$/', $s, $r)) {
        $param_name = strtoupper($r[1]);
        $param_val = $r[2];
        if ($param_name == 'NAME') {
          $lang['name'] = $param_val;
        } elseif ($param_name == 'DEFAULT') {
          $lang['default'] = strtolower($param_val);
        } else {
          print("Warning: Unknown parameter: $param_name in file $fn at line $str_num\r\n" );
        }
      } elseif (preg_match('/^\s*([_A-Za-z][_A-Za-z0-9]*)\s*=\s*+(.*+)\s*$/', $s, $r)) {
        $str_name = $r[1];
        $str_val = $r[2];
        if (isset($lang['strings'][$str_name])) print("Warning: Duplicated definition for $str_name in file $fn at line $str_num\r\n");
        $lang['strings'][$str_name] = $str_val;
      }
    }
    fclose($f);

    if (!isset($lang['name'])) {
      $lang['name'] = $lang_code;
      print("Warning: NAME parameter is not specified in $fn. $lang_code assumed\r\n");
    }

  }

  // Проверка ссылок и отсутствие цикличности
  $lang_abc = Array();
  foreach($langs as $lang_code => &$lang) {
    $lang_abc[] = $lang_code;
    if (isset($lang['default'])) {
      $cyclecheck = Array($lang_code => 1);
      $this_lang = $lang_code;
      do {
        $ref_lang = $langs[$this_lang]['default'];
        if (isset($cyclecheck[$ref_lang])) {
          $reflink = '';
          foreach($cyclecheck as $k => $v) {
            $reflink .= $k . '->';
          }
          $reflink .= $ref_lang;
          die("ERROR: Cyclic default language reference: $reflink\r\n");
        }
        $cyclecheck[$ref_lang] = 1;
        if (!isset($langs[$ref_lang])) {
          die("ERROR: Language $this_lang references unknown default language: $ref_lang\r\n");
        }
        $this_lang = $ref_lang;
      } while (isset($langs[$this_lang]['default']));

      $all_refs = Array();
      $r = $lang['default'];
      do {
        $all_refs[] = $r;
        $r = isset($langs[$r]['default']) ? $langs[$r]['default'] : false;
      } while ($r !== false);
      foreach($lang['strings'] as $n => $v) {
        $found = false;
        foreach($all_refs as $l) {
          if (isset($langs[$l]['strings'][$n])) {
            $found = true;
            break;
          }
        }
        if (!$found) print("Warning: String code $n in language $lang_code is not presented in the referenced language\r\n");
      }
    }
  }

  // Построение последовательности языков
  foreach($langs as $lang_code => &$lang) {
    $lang['strings']['language_name'] = $lang['name'];
  }

  sort($lang_abc);
  $lang_ordered = Array();

  $done = true;
  foreach ($lang_abc as $n) {
    if (!isset($langs[$n]['default'])) {
      $lang_ordered[$n] = $langs[$n];
    } else {
      $done = false;
    }
  }
  while (!$done) {
    $done = true;
    foreach ($lang_abc as $n) {
      if (!isset($lang_ordered[$n])) {
        if (isset($lang_ordered[$langs[$n]['default']])) {
          $lang_ordered[$n] = $langs[$n];
        } else {
          $done = false;
        }
      }
    }
  }
  $langs = $lang_ordered;

  $all_string_codes = Array();
  $all_string_values = Array();
  $encoded_strings = Array();

  foreach($langs as $lang_code => &$lang) {
    foreach($lang['strings'] as $n => $v) {
      $all_string_codes[$n] = 1;
      $string_var_name = ($v == '') ? 'strempty' : 'strval_' . $n . '_' . $lang_code;

      $encoded = '';
      $l = strlen($v);
      $i = 0;
      $unmapped = '';
      $hex_escape = false;
      while ($i < $l) {
        $ch = $v[$i++];
        $o = ord($ch);
        if (($o & 0x80) != 0) {
          if (($o & 0xE0) == 0xC0) $c = 1;
          elseif (($o & 0xF0) == 0xE0) $c = 2; 
          elseif (($o & 0xF8) == 0xF0) $c = 3; 
          else die("ERROR: Incorrect UTF8 literal 0x". dechex($o). " lang $lang_code string $n position $i\r\n");
          if (($i + $c) > $l) die("ERROR: End of line in UTF8 literal, lang $lang_code string $n position $i\r\n");
          while ($c-- > 0) $ch .= $v[$i++];
          if (!isset($chars_map[$ch])) {
            $encoded .= '?';
            $unmapped .= $ch;
            $hex_escape = false;
          } else { 
            $mapped = $chars_map[$ch];;
            $hex_escape = preg_match('/\\\\x[0-9A-Fa-f]+$/', $mapped);
            $encoded .= $mapped;
          }
        } else {
          if ($hex_escape && preg_match('/^[0-9A-Fa-f]$/', $ch)) $encoded .= '""';
          $encoded .= $ch;
          $hex_escape = false;
        }
      }
      if ($unmapped !== '') {
        print("Warning: Unmapped characters: $unmapped lang $lang_code string $n: $v\r\n");
      }


      if (!isset($all_string_values[$v])) {
        $all_string_values[$v] = $string_var_name;
        $encoded_strings[$string_var_name] = $encoded;
      }
    }
  }


  // Проверка существования всех строк во всех базовых языках
  foreach($langs as $lang_code => &$lang) {
    if (!isset($lang['default'])) {
      foreach($all_string_codes as $str_code => $v) {
        if (!isset($lang['strings'][$str_code])) {
          print("Warning: String code $str_code is not presented in base language $lang_code\r\n");
        }
      }
    }
  }


  print("Loaded " . count($langs) . " language files; ". count($all_string_codes) . " entries with " . count($all_string_values) . " unique values.\r\n");
  print("Generating localization sources...\r\n");
////////////////////////////// H //////////////////////////////////////////

  $h_only_name = pathinfo($output_h_filename, PATHINFO_BASENAME);

  $h_macros = preg_replace('#[^A-Z]#', '_', strtoupper($h_only_name)) . '_';

  $default_lang_const = '0';
  if ($default_lang !== false) {
    if (!isset($langs[$default_lang])) {
      print("Warning: Wrong language specified as default ($default_lang)\r\n");
    } else {
      $default_lang_const = 'LANGUAGE_' . strtoupper($default_lang);
    }
  }

  $fout = fopen($output_h_filename, 'w') or die("ERROR: Cannot write file $output_h_filename\r\n");
  fputs($fout, 
"// This file is generated automatically
// Language files max timestamp " . date('j.n.Y H:i:s', $maxmtime) ."
// Generated at " . date('j.n.Y H:i:s') . "

#ifndef $h_macros
#define $h_macros

#include <avr/io.h>
#include <avr/pgmspace.h>

#define LANGUAGE_COUNT " . count($langs) . "\r\n\r\n");
  
  $c = 0;
  foreach ($langs as $code => &$lang) {
    fputs($fout, "#define LANGUAGE_" . strtoupper($code) . " " . ($c++) . "\r\n");
  }
  fputs($fout, "\r\n#define LANGUAGE_DEFAULT $default_lang_const\r\n");
  fputs($fout, "\r\ntypedef struct {\r\n");
  foreach ($langs as $code => &$lang) {
    fputs($fout, "  PGM_P $code;\r\n");
  }

  fputs($fout, "} LocalizedString;\r\n\r\n");

  foreach($all_string_codes as $c => $v) {
    $all_vals = '';
    foreach($langs as $lang_code => &$lang) {
       if (isset($lang['strings'][$c])) {
         if ($all_vals != '') $all_vals .= '; ';
         $all_vals .= $lang_code . ': ' . $lang['strings'][$c];
       }
    }
    fputs($fout, "extern PROGMEM LocalizedString const str_$c; // $all_vals\r\n");
  }

  fputs($fout, "
// Возвращает код текущего выбранного языка
uint8_t get_language();

// Устанавливает заданный язык текущим
void set_language(uint8_t lang_code);

// В зависимости от выбранного текущего языка, возвращает указатель на строку из структуры локализации
PGM_P get_string(const LocalizedString * p_localized);

// В зависимости от указанного языка, возвращает указатель на строку из структуры локализации
PGM_P get_string_for(const LocalizedString * p_localized, uint8_t lang_code);

#endif /* $h_macros */
");

  fclose($fout);

  touch($output_h_filename, $maxmtime);

////////////////////////////// C //////////////////////////////////////////

  $fout = fopen($output_c_filename, 'w') or die("ERROR: Cannot write file $output_c_filename\r\n");

  fputs($fout, 
"// This file is generated automatically
// Language files max timestamp " . date('j.n.Y H:i:s', $maxmtime) ."
// Generated at " . date('j.n.Y H:i:s') . "

#include \"$h_only_name\"

static uint8_t selected_lang = LANGUAGE_DEFAULT;

");

  foreach($all_string_values as $val => $var_name) {
    $enc = $encoded_strings[$var_name];
    fputs($fout, "PROGMEM char const ".$var_name."[] = \"$enc\"; " . (($enc != $val) ? "// $val" : '') . "\r\n");
  }

  fputs($fout, "\r\n");

  foreach($all_string_codes as $c => $v) {
    $all_vals = '';
    foreach($langs as $lang_code => &$lang) {
       if (isset($lang['strings'][$c])) {
         if ($all_vals != '') $all_vals .= ', ';
         $all_vals .= '.' . $lang_code . ' = ' . $all_string_values[$lang['strings'][$c]];
       }
    }
    fputs($fout, "PROGMEM LocalizedString const str_$c = { $all_vals };\r\n");
  }

  fputs($fout, "

uint8_t get_language() {
  return selected_lang;
}

void set_language(uint8_t lang_code) {
  if (lang_code < LANGUAGE_COUNT) selected_lang = lang_code;
}

PGM_P get_string_for(const LocalizedString * p_localized, uint8_t lang_code) {
  PGM_P ptr;
  switch (lang_code) {
");
  foreach($langs as $lang_code => &$lang) {
    fputs($fout, "    case LANGUAGE_" . strtoupper($lang_code) . ":\r\n");
    $lg = $lang_code;
    for(;;) {
      fputs($fout, "      ptr = pgm_read_ptr(&p_localized->$lg);\r\n");

      if (!isset($langs[$lg]['default'])) {
        fputs($fout, "      return ptr;\r\n");
        break;
      } else {
        fputs($fout, "      if (ptr) return ptr;\r\n");
        $lg = $langs[$lg]['default'];
      }
    }
  }
  fputs($fout, "  }
  return 0;
}

PGM_P get_string(const LocalizedString * p_localized) {
  return get_string_for(p_localized, selected_lang);
}
");

  fclose($fout);

  touch($output_c_filename, $maxmtime);

  print("Localization files generated\r\n");
