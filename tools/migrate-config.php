<?php
/*
 * Legacy Kangle XML configuration migrator.
 *
 * This file is invoked by migrate-config.sh.  It intentionally supports
 * PHP 5.6 syntax because older Kangle installations commonly ship PHP 5.x.
 */

if (PHP_SAPI !== 'cli') {
    fwrite(STDERR, "This program must be run from the command line.\n");
    exit(1);
}

function usage($exitCode)
{
    $text = <<<'HELP'
Usage:
  migrate-config.sh [options]

Default operation:
  - converts /vhs/kangle/etc/config.xml in place;
  - converts /home/ftp/\*/\*/access.xml (site-root files only);
  - updates every /vhs/kangle/ext/php*/config.xml <cmd> lifetime;
  - removes obsolete built-in JS/WebP/WAF test files and vh_db.xml;
  - stops and removes CDNBest, its startup service, cdn_home, and binaries;
  - does not create backups or roll back completed operations on failure.

Options:
  --kangle-dir DIR      Kangle installation directory (default: /vhs/kangle)
  --config FILE         Main config.xml path
  --template FILE       New config-default.xml used to fill defaults
  --ftp-root DIR        Parent of the two-level site directories (default: /home/ftp)
  --access-file FILE    Convert an additional access.xml; may be repeated
  --no-main             Do not convert the main config.xml
  --no-sites            Do not scan --ftp-root
  --dry-run             Parse and report changes without writing files
  --output-dir DIR      Write converted copies below DIR instead of replacing
  --reload              Run "bin/kangle -r" after a successful in-place update
  -h, --help            Show this help

Examples:
  # Preview the real migration
  ./migrate-config.sh --dry-run

  # Convert the demo files into /tmp/kangle-converted without changing them
  ./migrate-config.sh --config /path/demo/config.xml --no-sites \
      --access-file /path/demo/access.xml --output-dir /tmp/kangle-converted

  # Convert the real files and gracefully reload Kangle
  ./migrate-config.sh --reload
HELP;
    fwrite($exitCode === 0 ? STDOUT : STDERR, $text . "\n");
    exit($exitCode);
}

function parseArguments($argv)
{
    $options = array(
        'kangle_dir' => '/vhs/kangle',
        'config' => null,
        'template' => null,
        'ftp_root' => '/home/ftp',
        'access_files' => array(),
        'main' => true,
        'sites' => true,
        'dry_run' => false,
        'output_dir' => null,
        'reload' => false,
    );

    $valueOptions = array(
        '--kangle-dir' => 'kangle_dir',
        '--config' => 'config',
        '--template' => 'template',
        '--ftp-root' => 'ftp_root',
        '--output-dir' => 'output_dir',
    );

    for ($i = 1; $i < count($argv); ++$i) {
        $arg = $argv[$i];
        if ($arg === '-h' || $arg === '--help') {
            usage(0);
        }
        if ($arg === '--no-main') {
            $options['main'] = false;
            continue;
        }
        if ($arg === '--no-sites') {
            $options['sites'] = false;
            continue;
        }
        if ($arg === '--dry-run') {
            $options['dry_run'] = true;
            continue;
        }
        if ($arg === '--reload') {
            $options['reload'] = true;
            continue;
        }
        if ($arg === '--access-file' || strpos($arg, '--access-file=') === 0) {
            if ($arg === '--access-file') {
                if (!isset($argv[++$i])) {
                    fwrite(STDERR, "error: --access-file requires a value\n");
                    usage(2);
                }
                $value = $argv[$i];
            } else {
                $value = substr($arg, strlen('--access-file='));
            }
            $options['access_files'][] = $value;
            continue;
        }

        $matched = false;
        foreach ($valueOptions as $longOption => $key) {
            if ($arg === $longOption) {
                if (!isset($argv[++$i])) {
                    fwrite(STDERR, "error: {$longOption} requires a value\n");
                    usage(2);
                }
                $options[$key] = $argv[$i];
                $matched = true;
                break;
            }
            if (strpos($arg, $longOption . '=') === 0) {
                $options[$key] = substr($arg, strlen($longOption) + 1);
                $matched = true;
                break;
            }
        }
        if ($matched) {
            continue;
        }
        fwrite(STDERR, "error: unknown option: {$arg}\n");
        usage(2);
    }

    $options['kangle_dir'] = rtrim($options['kangle_dir'], '/');
    if ($options['config'] === null) {
        $options['config'] = $options['kangle_dir'] . '/etc/config.xml';
    }
    if ($options['template'] === null) {
        $options['template'] = $options['kangle_dir'] . '/etc/config-default.xml';
    }
    if ($options['output_dir'] !== null) {
        $options['output_dir'] = rtrim($options['output_dir'], '/');
    }
    if (!$options['main'] && !$options['sites'] && count($options['access_files']) === 0) {
        fwrite(STDERR, "error: no input files were selected\n");
        exit(2);
    }
    return $options;
}

function addWarning(&$warnings, $message)
{
    if (!in_array($message, $warnings, true)) {
        $warnings[] = $message;
    }
}

function bump(&$stats, $name, $count)
{
    if (!isset($stats[$name])) {
        $stats[$name] = 0;
    }
    $stats[$name] += ($count === null ? 1 : $count);
}

function toUtf8($data, $path, &$warnings)
{
    $declaredEncoding = null;
    if (preg_match('/^\s*<\?xml[^>]*\bencoding\s*=\s*(["\'])([^"\']+)\1/i', $data, $match)) {
        $declaredEncoding = strtoupper(trim($match[2]));
    }
    $declaresLegacyChinese = in_array($declaredEncoding, array(
        'GBK', 'GB2312', 'GB18030', 'CP936', 'WINDOWS-936',
    ), true);
    if (!$declaresLegacyChinese && preg_match('//u', $data)) {
        return $data;
    }
    $converted = false;
    if (function_exists('iconv')) {
        $converted = @iconv('GB18030', 'UTF-8//IGNORE', $data);
    }
    if ($converted === false && function_exists('mb_convert_encoding')) {
        $converted = @mb_convert_encoding($data, 'UTF-8', 'GB18030,GBK,CP936');
    }
    if ($converted === false || !preg_match('//u', $converted)) {
        throw new RuntimeException("{$path}: content is neither valid UTF-8 nor convertible GB18030/GBK");
    }
    $converted = preg_replace('/<\?xml([^>]*?)encoding\s*=\s*(["\'])[^"\']*\2([^>]*)\?>/i', '<?xml$1encoding="UTF-8"$3?>', $converted, 1);
    addWarning($warnings, 'input encoding was converted from GB18030/GBK to UTF-8');
    return $converted;
}

function loadXmlFile($path, &$raw, &$warnings)
{
    $raw = @file_get_contents($path);
    if ($raw === false) {
        throw new RuntimeException("cannot read {$path}");
    }
    $raw = toUtf8($raw, $path, $warnings);
    $oldUseErrors = libxml_use_internal_errors(true);
    $doc = new DOMDocument('1.0', 'UTF-8');
    $doc->preserveWhiteSpace = false;
    $doc->formatOutput = true;
    $flags = LIBXML_NONET | LIBXML_NOBLANKS;
    $loaded = $doc->loadXML($raw, $flags);
    if (!$loaded) {
        $parts = array();
        foreach (libxml_get_errors() as $error) {
            $parts[] = trim($error->message) . ' at line ' . $error->line;
        }
        libxml_clear_errors();
        libxml_use_internal_errors($oldUseErrors);
        throw new RuntimeException("{$path}: invalid XML: " . implode('; ', $parts));
    }
    libxml_clear_errors();
    libxml_use_internal_errors($oldUseErrors);
    if (!$doc->documentElement || $doc->documentElement->tagName !== 'config') {
        throw new RuntimeException("{$path}: root element must be <config>");
    }
    return $doc;
}

function directElements($parent, $tagName)
{
    $result = array();
    foreach ($parent->childNodes as $node) {
        if ($node instanceof DOMElement && ($tagName === null || $node->tagName === $tagName)) {
            $result[] = $node;
        }
    }
    return $result;
}

function firstDirectElement($parent, $tagName)
{
    $nodes = directElements($parent, $tagName);
    return count($nodes) > 0 ? $nodes[0] : null;
}

function firstDirectByAttribute($parent, $tagName, $attribute, $value)
{
    foreach (directElements($parent, $tagName) as $node) {
        if ($node->getAttribute($attribute) === $value) {
            return $node;
        }
    }
    return null;
}

function insertRootConfiguration($root, $node)
{
    foreach ($root->childNodes as $child) {
        if (!($child instanceof DOMElement)) {
            continue;
        }
        if (in_array($child->tagName, array('request', 'response', 'vhs', 'vh'), true)) {
            $root->insertBefore($node, $child);
            return;
        }
    }
    $root->appendChild($node);
}

function ensureRootElement($doc, $root, $tagName, $attributes, &$stats)
{
    $node = firstDirectElement($root, $tagName);
    if (!$node) {
        $node = $doc->createElement($tagName);
        insertRootConfiguration($root, $node);
        bump($stats, 'added <' . $tagName . '>', 1);
    }
    foreach ($attributes as $name => $value) {
        if (!$node->hasAttribute($name)) {
            $node->setAttribute($name, $value);
            bump($stats, 'filled ' . $tagName . ' attributes', 1);
        }
    }
    return $node;
}

function nodeHasElementChildren($node)
{
    foreach ($node->childNodes as $child) {
        if ($child instanceof DOMElement) {
            return true;
        }
    }
    return false;
}

function removeTextChildren($node)
{
    $remove = array();
    foreach ($node->childNodes as $child) {
        if ($child instanceof DOMText || $child instanceof DOMCdataSection) {
            $remove[] = $child;
        }
    }
    foreach ($remove as $child) {
        $node->removeChild($child);
    }
}

function disableUnsupportedModule($doc, $node, $module, &$stats, &$warnings)
{
    $raw = trim($doc->saveXML($node));
    $raw = str_replace('--', '- -', $raw);
    $comment = $doc->createComment(' migration: unsupported module "' . $module . '" disabled; original: ' . $raw . ' ');
    $node->parentNode->replaceChild($comment, $node);
    bump($stats, 'disabled unsupported modules', 1);
    addWarning($warnings, "module '{$module}' has no current equivalent and was preserved as an XML comment");
}

function normalizeRuleModules($doc, &$stats, &$warnings, &$modules)
{
    $xpath = new DOMXPath($doc);
    $nodes = array();
    foreach ($xpath->query('//chain/*') as $node) {
        if ($node instanceof DOMElement) {
            $nodes[] = $node;
        }
    }

    $aliases = array(
        'method' => 'meth',
        'file_name' => 'filename',
        'reg_file_name' => 'reg_filename',
        // Some old configurations used the implementation class name even
        // though the public module name has always been "content".
        'reg_content' => 'content',
    );
    $unsupportedMarks = array(
        'self_ip' => true,
    );
    $textAttributes = array(
        'acl:url' => 'url',
        'acl:path' => 'path',
        'acl:reg_path' => 'path',
        'acl:reg_param' => 'param',
        'acl:host' => 'v',
        'acl:wide_host' => 'v',
        'acl:multi_host' => 'v',
        'acl:src' => 'ip',
        'acl:srcs' => 'v',
        'acl:self' => 'ip',
        'acl:selfs' => 'v',
        'acl:self_port' => 'port',
        'acl:self_ports' => 'v',
        'acl:listen_ports' => 'v',
        'acl:dst_port' => 'port',
        'acl:meth' => 'meth',
        'acl:time' => 'time',
        'acl:file_ext' => 'v',
        'acl:file' => 'v',
        'acl:filename' => 'v',
        'acl:reg_file' => 'file',
        'acl:reg_filename' => 'filename',
        'acl:dir' => 'v',
        'acl:header' => 'val',
        'acl:auth_user' => 'v',
        'acl:ssl_serial' => 'v',
        'mark:map_redirect' => 'v',
        'mark:parent' => 'val',
        'mark:timeout' => 'v',
        'mark:mark' => 'v',
    );

    foreach ($nodes as $node) {
        $type = null;
        $module = null;
        $tag = $node->tagName;
        if (strpos($tag, 'acl_') === 0) {
            $type = 'acl';
            $module = substr($tag, 4);
        } elseif (strpos($tag, 'mark_') === 0) {
            $type = 'mark';
            $module = substr($tag, 5);
        } elseif ($tag === 'acl' || $tag === 'mark') {
            $type = $tag;
            $module = $node->getAttribute('module');
        } else {
            continue;
        }

        if ($module === 'named' && $node->hasAttribute('ref')) {
            $module = '';
        }
        if (isset($aliases[$module])) {
            $module = $aliases[$module];
            bump($stats, 'renamed legacy module aliases', 1);
        }

        if ($tag !== 'acl' && $tag !== 'mark') {
            $replacement = $doc->createElement($type);
            foreach ($node->attributes as $attribute) {
                $replacement->setAttribute($attribute->name, $attribute->value);
            }
            if ($module !== '') {
                $replacement->setAttribute('module', $module);
            } else {
                $replacement->removeAttribute('module');
            }
            while ($node->firstChild) {
                $replacement->appendChild($node->firstChild);
            }
            $node->parentNode->replaceChild($replacement, $node);
            $node = $replacement;
            bump($stats, 'converted legacy ACL/Mark nodes', 1);
        } elseif ($module !== '' && $node->getAttribute('module') !== $module) {
            $node->setAttribute('module', $module);
        }

        if ($module === '') {
            continue;
        }
        $modules[$module] = true;
        if ($type === 'mark' && isset($unsupportedMarks[$module])) {
            disableUnsupportedModule($doc, $node, $module, $stats, $warnings);
            continue;
        }

        $key = $type . ':' . $module;
        if (isset($textAttributes[$key]) && !$node->hasAttribute($textAttributes[$key]) && !nodeHasElementChildren($node)) {
            $value = trim($node->textContent);
            if ($value !== '') {
                $node->setAttribute($textAttributes[$key], $value);
                removeTextChildren($node);
                bump($stats, 'moved legacy text values to attributes', 1);
            }
        }
    }
}

function migrateRequestVhsActions($doc, &$stats)
{
    $xpath = new DOMXPath($doc);
    foreach ($xpath->query('//request[@action="vhs"] | //request//chain[@action="vhs"]') as $node) {
        $node->setAttribute('action', 'allow');
        bump($stats, 'converted request vhs actions', 1);
    }
}

function removeLegacyCaptchaProxyMarks($doc, &$stats)
{
    $xpath = new DOMXPath($doc);
    foreach ($xpath->query('//request//chain') as $chain) {
        if (!($chain instanceof DOMElement)) {
            continue;
        }
        $isCaptchaRule = false;
        foreach (directElements($chain, 'acl') as $acl) {
            if ($acl->getAttribute('module') === 'path' &&
                $acl->getAttribute('path') === '/KANGLE_CCIMG.php') {
                $isCaptchaRule = true;
                break;
            }
        }
        if (!$isCaptchaRule) {
            continue;
        }
        foreach (directElements($chain, 'mark') as $mark) {
            if ($mark->getAttribute('module') === 'host') {
                $chain->removeChild($mark);
                bump($stats, 'removed legacy captcha proxy marks', 1);
            }
        }
    }
}

function removeDirectElements($root, $tagName)
{
    $nodes = directElements($root, $tagName);
    foreach ($nodes as $node) {
        $root->removeChild($node);
    }
    return $nodes;
}

function firstNonEmptyText($nodes)
{
    foreach ($nodes as $node) {
        $value = trim($node->textContent);
        if ($value !== '') {
            return $value;
        }
    }
    return null;
}

function migrateTimeout($doc, $root, &$stats)
{
    $timeout = firstDirectElement($root, 'timeout');
    if (!$timeout) {
        $timeout = $doc->createElement('timeout');
        $timeout->setAttribute('rw', '60');
        insertRootConfiguration($root, $timeout);
        bump($stats, 'added <timeout>', 1);
    } elseif (!$timeout->hasAttribute('rw')) {
        $value = trim($timeout->textContent);
        if ($value === '') {
            $value = '60';
        }
        $timeout->setAttribute('rw', $value);
        removeTextChildren($timeout);
        bump($stats, 'converted legacy timeout', 1);
    }
    $legacy = removeDirectElements($root, 'connect_timeout');
    $connectTimeout = firstNonEmptyText($legacy);
    if ($connectTimeout !== null && !$timeout->hasAttribute('connect')) {
        $timeout->setAttribute('connect', $connectTimeout);
    }
    if (!$timeout->hasAttribute('connect')) {
        $timeout->setAttribute('connect', '10');
        bump($stats, 'filled timeout attributes', 1);
    }
    if (count($legacy) > 0) {
        bump($stats, 'migrated connect_timeout', count($legacy));
    }
}

function migrateRunAs($doc, $root, &$stats)
{
    $runAs = firstDirectElement($root, 'run_as');
    $legacy = removeDirectElements($root, 'run');
    if (!$runAs) {
        $runAs = $doc->createElement('run_as');
        if (count($legacy) > 0) {
            foreach ($legacy[0]->attributes as $attribute) {
                $runAs->setAttribute($attribute->name, $attribute->value);
            }
        }
        insertRootConfiguration($root, $runAs);
        bump($stats, count($legacy) > 0 ? 'converted <run> to <run_as>' : 'added <run_as>', 1);
    } elseif (count($legacy) > 0) {
        bump($stats, 'removed duplicate legacy <run>', count($legacy));
    }
    foreach (array('user', 'group') as $attribute) {
        if (!$runAs->hasAttribute($attribute)) {
            $runAs->setAttribute($attribute, '');
            bump($stats, 'filled run_as attributes', 1);
        }
    }
}

function migrateWorkers($doc, $root, &$stats, &$warnings)
{
    $legacyDns = removeDirectElements($root, 'worker_dns');
    $dnsValue = firstNonEmptyText($legacyDns);
    $dns = ensureRootElement($doc, $root, 'dns', array(), $stats);
    if (!$dns->hasAttribute('worker')) {
        $dns->setAttribute('worker', $dnsValue !== null ? $dnsValue : '8');
    }
    if (count($legacyDns) > 0) {
        bump($stats, 'migrated worker_dns', count($legacyDns));
    }

    $legacyIo = removeDirectElements($root, 'worker_io');
    $legacyMaxIo = removeDirectElements($root, 'max_io');
    $ioWorker = firstNonEmptyText($legacyIo);
    $ioMax = firstNonEmptyText($legacyMaxIo);
    $io = ensureRootElement($doc, $root, 'io', array(), $stats);
    if (!$io->hasAttribute('worker')) {
        $io->setAttribute('worker', $ioWorker !== null ? $ioWorker : '2');
    }
    if (!$io->hasAttribute('max')) {
        $io->setAttribute('max', $ioMax !== null ? $ioMax : '0');
    }
    if (!$io->hasAttribute('buffer')) {
        $io->setAttribute('buffer', '256K');
    }
    if (count($legacyIo) + count($legacyMaxIo) > 0) {
        bump($stats, 'migrated legacy I/O worker settings', count($legacyIo) + count($legacyMaxIo));
    }

    $ioTimeout = removeDirectElements($root, 'io_timeout');
    if (count($ioTimeout) > 0) {
        bump($stats, 'removed obsolete io_timeout', count($ioTimeout));
        addWarning($warnings, 'io_timeout is unsupported and has no direct replacement; timeout@rw controls request I/O timeout');
    }
    ensureRootElement($doc, $root, 'fiber', array('stack_size' => '0'), $stats);
}

function migrateConnect($doc, $root, &$stats)
{
    $legacy = removeDirectElements($root, 'keep_alive_count');
    $legacyValue = firstNonEmptyText($legacy);
    $connect = ensureRootElement($doc, $root, 'connect', array(), $stats);
    if (!$connect->hasAttribute('max_keep_alive')) {
        $connect->setAttribute('max_keep_alive', $legacyValue !== null ? $legacyValue : '0');
    }
    foreach (array('max' => '0', 'max_per_ip' => '0', 'per_ip_deny' => '0') as $name => $value) {
        if (!$connect->hasAttribute($name)) {
            $connect->setAttribute($name, $value);
        }
    }
    if (count($legacy) > 0) {
        bump($stats, 'migrated keep_alive_count', count($legacy));
    }
}

function migrateCompress($doc, $root, &$stats)
{
    $compress = ensureRootElement($doc, $root, 'compress', array(), $stats);
    $aliases = array(
        'only_compress_cache' => 'only_cache',
        'only_gzip_cache' => 'only_cache',
        'min_compress_length' => 'min_length',
        'min_gzip_length' => 'min_length',
    );
    foreach ($aliases as $old => $new) {
        if ($compress->hasAttribute($old)) {
            if (!$compress->hasAttribute($new)) {
                $compress->setAttribute($new, $compress->getAttribute($old));
            }
            $compress->removeAttribute($old);
            bump($stats, 'renamed legacy compress attributes', 1);
        }
    }
    foreach (array('default', 'only_gzip') as $obsolete) {
        if ($compress->hasAttribute($obsolete)) {
            $compress->removeAttribute($obsolete);
            bump($stats, 'removed obsolete compress attributes', 1);
        }
    }
    foreach (array('only_cache' => '0', 'min_length' => '512', 'gzip_level' => '5', 'br_level' => '5', 'zstd_level' => '5') as $name => $value) {
        if (!$compress->hasAttribute($name)) {
            $compress->setAttribute($name, $value);
            bump($stats, 'filled compress attributes', 1);
        }
    }
}

function migrateCacheAndLog($doc, $root, &$stats)
{
    ensureRootElement($doc, $root, 'cache', array(
        'default' => '1',
        'max_cache_size' => '10M',
        'memory' => '1G',
        'refresh_time' => '30',
        'disk' => '0',
        'max_bigobj_size' => '1G',
        'cache_part' => '1',
    ), $stats);
    ensureRootElement($doc, $root, 'log', array(
        'level' => '2',
        'rotate_size' => '100M',
        'error_rotate_size' => '100M',
        'logs_size' => '1G',
    ), $stats);
}

function migrateFirewall($doc, $root, &$stats, &$warnings)
{
    $legacyMap = array(
        'bl_time' => 'bl_time',
        'block_ip_cmd' => 'block_ip_cmd',
        'unblock_ip_cmd' => 'unblock_ip_cmd',
        'flush_ip_cmd' => 'flush_ip_cmd',
        'report_url' => 'report_url',
    );
    $values = array();
    foreach ($legacyMap as $tag => $attribute) {
        $nodes = removeDirectElements($root, $tag);
        $value = firstNonEmptyText($nodes);
        if ($value !== null) {
            $values[$attribute] = $value;
        }
        if (count($nodes) > 0) {
            bump($stats, 'migrated legacy firewall elements', count($nodes));
        }
    }

    $attackNodes = removeDirectElements($root, 'attack');
    foreach ($attackNodes as $attack) {
        if ($attack->hasAttribute('wl_time')) {
            $values['wl_time'] = $attack->getAttribute('wl_time');
        }
        if ($attack->hasAttribute('path_info') && !firstDirectElement($root, 'path_info')) {
            $pathInfo = $doc->createElement('path_info', $attack->getAttribute('path_info'));
            insertRootConfiguration($root, $pathInfo);
        }
        $other = array();
        foreach ($attack->attributes as $attribute) {
            if ($attribute->name !== 'wl_time' && $attribute->name !== 'path_info') {
                $other[] = $attribute->name;
            }
        }
        if (count($other) > 0) {
            addWarning($warnings, 'obsolete <attack> attributes were removed: ' . implode(', ', $other));
        }
    }
    if (count($attackNodes) > 0) {
        bump($stats, 'removed obsolete <attack>', count($attackNodes));
    }

    $firewall = firstDirectElement($root, 'firewall');
    if (!$firewall) {
        $firewall = $doc->createElement('firewall');
        insertRootConfiguration($root, $firewall);
        bump($stats, 'added <firewall>', 1);
    }
    foreach ($values as $name => $value) {
        if (!$firewall->hasAttribute($name)) {
            $firewall->setAttribute($name, $value);
        }
    }
    if (!$firewall->hasAttribute('bl_time')) {
        $firewall->setAttribute('bl_time', '1800');
        bump($stats, 'filled firewall attributes', 1);
    }
    if (!$firewall->hasAttribute('wl_time')) {
        $firewall->setAttribute('wl_time', '1800');
        bump($stats, 'filled firewall attributes', 1);
    }
}

function ensureGlobalFlagChain($doc, $root, $attributes, &$stats)
{
    if (count($attributes) === 0) {
        return;
    }
    $request = firstDirectElement($root, 'request');
    if (!$request) {
        $request = $doc->createElement('request');
        $request->setAttribute('action', 'allow');
        insertRootConfiguration($root, $request);
    }
    $begin = firstDirectByAttribute($request, 'table', 'name', 'BEGIN');
    if (!$begin) {
        $begin = $doc->createElement('table');
        $begin->setAttribute('name', 'BEGIN');
        $request->appendChild($begin);
    }
    $target = null;
    foreach (directElements($begin, 'chain') as $chain) {
        if ($chain->getAttribute('action') !== 'continue') {
            continue;
        }
        $elements = directElements($chain, null);
        if (count($elements) === 1 && $elements[0]->tagName === 'mark' && $elements[0]->getAttribute('module') === 'flag') {
            $target = $elements[0];
            break;
        }
    }
    if (!$target) {
        $chain = $doc->createElement('chain');
        $chain->setAttribute('action', 'continue');
        $target = $doc->createElement('mark');
        $target->setAttribute('module', 'flag');
        $chain->appendChild($target);
        if ($begin->firstChild) {
            $begin->insertBefore($chain, $begin->firstChild);
        } else {
            $begin->appendChild($chain);
        }
    }
    foreach ($attributes as $name => $value) {
        $target->setAttribute($name, $value);
    }
    bump($stats, 'migrated global proxy flags', count($attributes));
}

function migrateObsoleteRootOptions($doc, $root, &$stats, &$warnings)
{
    $flagAttributes = array();
    $viaNodes = removeDirectElements($root, 'insert_via');
    $via = firstNonEmptyText($viaNodes);
    if ($via !== null && ($via === '1' || strtolower($via) === 'on')) {
        $flagAttributes['via'] = '1';
    }
    if (count($viaNodes) > 0) {
        bump($stats, 'removed legacy insert_via', count($viaNodes));
    }

    $xffNodes = removeDirectElements($root, 'x_forwarded_for');
    $xff = firstNonEmptyText($xffNodes);
    if ($xff !== null && !($xff === '1' || strtolower($xff) === 'on')) {
        $flagAttributes['no_x_forwarded_for'] = '1';
    }
    if (count($xffNodes) > 0) {
        bump($stats, 'removed legacy x_forwarded_for', count($xffNodes));
    }
    ensureGlobalFlagChain($doc, $root, $flagAttributes, $stats);

    $obsolete = array('charset');
    foreach ($obsolete as $tag) {
        $nodes = removeDirectElements($root, $tag);
        if (count($nodes) > 0) {
            bump($stats, 'removed obsolete root elements', count($nodes));
            addWarning($warnings, "obsolete root element <{$tag}> was removed");
        }
    }
}

function ensureNamedRootNode($doc, $root, $tagName, $name, $attributes, &$stats)
{
    $node = firstDirectByAttribute($root, $tagName, 'name', $name);
    if (!$node) {
        $node = $doc->createElement($tagName);
        $node->setAttribute('name', $name);
        insertRootConfiguration($root, $node);
        bump($stats, 'added ' . $tagName . ':' . $name, 1);
    }
    foreach ($attributes as $attribute => $value) {
        if (!$node->hasAttribute($attribute)) {
            $node->setAttribute($attribute, $value);
            bump($stats, 'filled ' . $tagName . ':' . $name . ' attributes', 1);
        }
    }
    return $node;
}

function extensionConfigContains($directory, $name)
{
    if (!is_dir($directory)) {
        return false;
    }
    $files = glob(rtrim($directory, '/') . '/*.xml');
    if ($files === false) {
        return false;
    }
    foreach ($files as $file) {
        $data = @file_get_contents($file);
        if ($data !== false && preg_match('/<dso_extend\b[^>]*\bname\s*=\s*(["\'])' . preg_quote($name, '/') . '\1/i', $data)) {
            return true;
        }
    }
    return false;
}

function migrateVhs($doc, $root, $templateDoc, &$stats)
{
    $vhs = firstDirectElement($root, 'vhs');
    if (!$vhs) {
        $vhs = $doc->createElement('vhs');
        insertRootConfiguration($root, $vhs);
        bump($stats, 'added <vhs>', 1);
    }

    $legacyErrorAttributes = array();
    foreach ($vhs->attributes as $attribute) {
        if (preg_match('/^error_([0-9]{3})$/', $attribute->name, $match)) {
            $legacyErrorAttributes[$attribute->name] = array($match[1], $attribute->value);
        }
    }
    foreach ($legacyErrorAttributes as $name => $entry) {
        if (!firstDirectByAttribute($vhs, 'error', 'code', $entry[0])) {
            $error = $doc->createElement('error');
            $error->setAttribute('code', $entry[0]);
            $error->setAttribute('file', $entry[1]);
            $vhs->appendChild($error);
        }
        $vhs->removeAttribute($name);
        bump($stats, 'converted vhs error attributes', 1);
    }

    foreach (directElements($vhs, 'vh_database') as $database) {
        $duplicate = false;
        foreach (directElements($root, 'vh_database') as $existing) {
            if ($existing->getAttribute('driver') === $database->getAttribute('driver') && $existing->getAttribute('dbname') === $database->getAttribute('dbname')) {
                $duplicate = true;
                break;
            }
        }
        if (!$duplicate) {
            $root->insertBefore($database->cloneNode(true), $vhs);
        }
        $vhs->removeChild($database);
        bump($stats, 'moved vh_database to config root', 1);
    }

    $templateVhs = $templateDoc ? firstDirectElement($templateDoc->documentElement, 'vhs') : null;
    if ($templateVhs) {
        foreach (directElements($templateVhs, null) as $child) {
            $keyAttribute = null;
            if ($child->tagName === 'error') {
                $keyAttribute = 'code';
            } elseif ($child->tagName === 'index') {
                $keyAttribute = 'file';
            } elseif ($child->tagName === 'mime_type') {
                $keyAttribute = 'ext';
            }
            if ($keyAttribute === null || !$child->hasAttribute($keyAttribute)) {
                continue;
            }
            if (!firstDirectByAttribute($vhs, $child->tagName, $keyAttribute, $child->getAttribute($keyAttribute))) {
                $vhs->appendChild($doc->importNode($child, true));
                bump($stats, 'merged vhs defaults', 1);
            }
        }
    } else {
        foreach (array('403', '404') as $code) {
            if (!firstDirectByAttribute($vhs, 'error', 'code', $code)) {
                $error = $doc->createElement('error');
                $error->setAttribute('code', $code);
                $error->setAttribute('file', '/' . $code . '.html');
                $vhs->appendChild($error);
                bump($stats, 'merged vhs defaults', 1);
            }
        }
        foreach (array('index.html', 'index.htm') as $file) {
            if (!firstDirectByAttribute($vhs, 'index', 'file', $file)) {
                $index = $doc->createElement('index');
                $index->setAttribute('file', $file);
                $vhs->appendChild($index);
                bump($stats, 'merged vhs defaults', 1);
            }
        }
        if (!firstDirectByAttribute($vhs, 'mime_type', 'ext', '*')) {
            $mime = $doc->createElement('mime_type');
            $mime->setAttribute('ext', '*');
            $mime->setAttribute('type', 'text/plain');
            $vhs->appendChild($mime);
            bump($stats, 'merged vhs defaults', 1);
        }
    }
}

function ensureCoreConfiguration($doc, $root, $templateDoc, $options, $modules, &$stats, &$warnings)
{
    if ($templateDoc) {
        foreach (array('programVersion', 'configVersion') as $attribute) {
            if (!$root->hasAttribute($attribute) && $templateDoc->documentElement->hasAttribute($attribute)) {
                $root->setAttribute($attribute, $templateDoc->documentElement->getAttribute($attribute));
                bump($stats, 'filled config version attributes', 1);
            }
        }
    }

    foreach (directElements($root, 'dso_extend') as $extension) {
        if (strcasecmp($extension->getAttribute('name'), 'filter') === 0) {
            $root->removeChild($extension);
            bump($stats, 'removed obsolete filter dso loader', 1);
        }
    }
    ensureNamedRootNode($doc, $root, 'api', 'webdav', array(
        'file' => 'bin/webdav.${dso}',
        'life_time' => '60',
        'max_error_count' => '5',
    ), $stats);
    ensureNamedRootNode($doc, $root, 'api', 'whm', array('file' => 'buildin:whm'), $stats);

    if (!is_file($options['kangle_dir'] . '/bin/webdav.so') && !is_file($options['kangle_dir'] . '/bin/webdav.dll')) {
        addWarning($warnings, 'WebDAV API was configured but its binary is not installed under KANGLE_DIR/bin');
    }

    if (!firstDirectElement($root, 'request')) {
        $request = $doc->createElement('request');
        $request->setAttribute('action', 'allow');
        insertRootConfiguration($root, $request);
        bump($stats, 'added <request>', 1);
    }
    if (!firstDirectElement($root, 'response')) {
        $response = $doc->createElement('response');
        $response->setAttribute('action', 'allow');
        insertRootConfiguration($root, $response);
        bump($stats, 'added <response>', 1);
    }
}

function migrateLegacyVirtualHostContainers($doc, &$stats)
{
    $root = $doc->documentElement;
    foreach (directElements($root, 'vhs') as $container) {
        foreach (directElements($container, 'vh') as $host) {
            $root->insertBefore($host, $container);
            bump($stats, 'moved legacy virtual hosts to config root', 1);
        }
        if (!$container->hasAttributes() && count(directElements($container, null)) === 0) {
            $root->removeChild($container);
        }
    }
}

function migrateMainDocument($doc, $templateDoc, $options, $externalModules, &$stats, &$warnings)
{
    migrateLegacyVirtualHostContainers($doc, $stats);
    $root = $doc->documentElement;
    $modules = $externalModules;
    migrateRequestVhsActions($doc, $stats);
    normalizeRuleModules($doc, $stats, $warnings, $modules);
    removeLegacyCaptchaProxyMarks($doc, $stats);
    migrateTimeout($doc, $root, $stats);
    migrateRunAs($doc, $root, $stats);
    migrateWorkers($doc, $root, $stats, $warnings);
    migrateConnect($doc, $root, $stats);
    migrateCompress($doc, $root, $stats);
    migrateCacheAndLog($doc, $root, $stats);
    migrateFirewall($doc, $root, $stats, $warnings);
    migrateObsoleteRootOptions($doc, $root, $stats, $warnings);
    ensureCoreConfiguration($doc, $root, $templateDoc, $options, $modules, $stats, $warnings);
    migrateVhs($doc, $root, $templateDoc, $stats);
}

function migrateAccessDocument($doc, &$stats, &$warnings, &$modules)
{
    migrateLegacyVirtualHostContainers($doc, $stats);
    migrateRequestVhsActions($doc, $stats);
    normalizeRuleModules($doc, $stats, $warnings, $modules);
    removeLegacyCaptchaProxyMarks($doc, $stats);
}

function migratePhpExtensionDocument($doc, &$stats, &$warnings)
{
    $xpath = new DOMXPath($doc);
    $commands = $xpath->query('//cmd');
    if ($commands->length === 0) {
        addWarning($warnings, 'PHP extension config contains no <cmd> element');
        return;
    }
    foreach ($commands as $command) {
        if (!($command instanceof DOMElement)) {
            continue;
        }
        foreach (array('idle_time' => '600', 'life_time' => '60') as $name => $value) {
            if ($command->getAttribute($name) !== $value) {
                $command->setAttribute($name, $value);
                bump($stats, 'updated PHP cmd lifetime attributes', 1);
            }
        }
    }
}

function discoverPhpExtensionConfigs($kangleDirectory, &$warnings)
{
    $pattern = rtrim($kangleDirectory, '/') . '/ext/php*/config.xml';
    $matches = glob($pattern);
    if ($matches === false) {
        throw new RuntimeException("cannot scan PHP extension configs: {$pattern}");
    }
    $files = array();
    foreach ($matches as $path) {
        // Do not follow a site-supplied link outside the Kangle installation.
        if (is_link($path) || is_link(dirname($path))) {
            addWarning($warnings, 'symbolic-link PHP extension config was skipped: ' . $path);
            continue;
        }
        if (is_file($path)) {
            $files[] = $path;
        }
    }
    sort($files, SORT_STRING);
    return $files;
}

function discoverObsoleteInstallationFiles($kangleDirectory, &$warnings)
{
    $relativePaths = array(
        'bin/autoupdate',
        'bin/cdnbest',
        'bin/daemon',
        'bin/js.so',
        'bin/testdso.so',
        'bin/wafdso.so',
        'bin/webp.so',
        'ext/js.dso.xml',
        'ext/webp.dso.xml',
        'etc/vh_db.xml',
    );
    $files = array();
    foreach ($relativePaths as $relativePath) {
        $path = rtrim($kangleDirectory, '/') . '/' . $relativePath;
        if (is_file($path) || is_link($path)) {
            $files[] = $path;
        } elseif (file_exists($path)) {
            addWarning($warnings, 'obsolete path is not a regular file and was not removed: ' . $path);
        }
    }
    return $files;
}

function findCdnBestProcessIds($kangleDirectory)
{
    $binary = rtrim($kangleDirectory, '/') . '/bin/cdnbest';
    $daemon = rtrim($kangleDirectory, '/') . '/bin/daemon';
    $pids = array();
    $processes = glob('/proc/[0-9]*/cmdline');
    if ($processes === false) {
        return $pids;
    }
    foreach ($processes as $cmdlinePath) {
        $data = @file_get_contents($cmdlinePath);
        if ($data === false || $data === '') {
            continue;
        }
        $args = explode("\0", rtrim($data, "\0"));
        if ($args[0] !== $binary && !($args[0] === $daemon && in_array($binary, $args, true))) {
            continue;
        }
        $pid = (int)basename(dirname($cmdlinePath));
        if ($pid > 1) {
            $pids[] = $pid;
        }
    }
    return $pids;
}

function discoverCdnBestRemoval($kangleDirectory)
{
    $serviceFiles = array();
    $seenServiceLocations = array();
    foreach (array(
        '/etc/systemd/system/cdnbest.service',
        '/lib/systemd/system/cdnbest.service',
        '/usr/lib/systemd/system/cdnbest.service',
        '/etc/init.d/cdnbest',
        '/etc/rc.d/init.d/cdnbest',
    ) as $path) {
        if (!is_file($path)) {
            continue;
        }
        $content = @file_get_contents($path);
        if ($content !== false && strpos($content, rtrim($kangleDirectory, '/') . '/bin/cdnbest') !== false) {
            $location = realpath(dirname($path)) . '/' . basename($path);
            if (!isset($seenServiceLocations[$location])) {
                $serviceFiles[] = $path;
                $seenServiceLocations[$location] = true;
            }
        }
    }

    $startupLinks = array();
    $seenStartupLocations = array();
    if (count($serviceFiles) > 0) {
        foreach (array('/etc', '/etc/rc.d') as $root) {
            for ($level = 0; $level <= 6; ++$level) {
                $matches = glob($root . '/rc' . $level . '.d/[SK][0-9][0-9]cdnbest');
                if ($matches === false) {
                    continue;
                }
                foreach ($matches as $path) {
                    if (is_link($path)) {
                        $location = realpath(dirname($path)) . '/' . basename($path);
                        if (!isset($seenStartupLocations[$location])) {
                            $startupLinks[] = $path;
                            $seenStartupLocations[$location] = true;
                        }
                    }
                }
            }
        }
    }

    $home = rtrim($kangleDirectory, '/') . '/cdn_home';
    return array(
        'service_files' => $serviceFiles,
        'startup_links' => $startupLinks,
        'home' => (is_dir($home) || is_link($home)) ? $home : null,
        'process_ids' => findCdnBestProcessIds($kangleDirectory),
    );
}

function availableProgram($paths)
{
    foreach ($paths as $path) {
        if (is_executable($path)) {
            return $path;
        }
    }
    return null;
}

function runCdnBestCommand($program, $arguments, &$detail)
{
    $parts = array(escapeshellarg($program));
    foreach ($arguments as $argument) {
        $parts[] = escapeshellarg($argument);
    }
    $lines = array();
    $exitCode = 0;
    exec(implode(' ', $parts) . ' 2>&1', $lines, $exitCode);
    $detail = implode('; ', $lines);
    return $exitCode === 0;
}

function signalCdnBestProcesses($kangleDirectory, $signal)
{
    foreach (findCdnBestProcessIds($kangleDirectory) as $pid) {
        if (function_exists('posix_kill')) {
            @posix_kill($pid, $signal);
        } else {
            $detail = '';
            runCdnBestCommand('/bin/kill', array('-' . $signal, (string)$pid), $detail);
        }
    }
}

function waitForCdnBestProcesses($kangleDirectory)
{
    for ($attempt = 0; $attempt < 20; ++$attempt) {
        if (count(findCdnBestProcessIds($kangleDirectory)) === 0) {
            return true;
        }
        usleep(100000);
    }
    return count(findCdnBestProcessIds($kangleDirectory)) === 0;
}

function removeCdnHome($kangleDirectory, $home)
{
    $expected = rtrim($kangleDirectory, '/') . '/cdn_home';
    if ($home !== $expected) {
        throw new RuntimeException('refusing to remove unexpected CDNBest directory: ' . $home);
    }
    if (is_link($home)) {
        if (!@unlink($home)) {
            throw new RuntimeException('cannot unlink CDNBest directory link: ' . $home);
        }
        return;
    }
    if (!is_dir($home)) {
        return;
    }
    $parent = realpath($kangleDirectory);
    if ($parent === false || realpath($home) !== $parent . '/cdn_home') {
        throw new RuntimeException('refusing to recursively remove CDNBest directory outside Kangle: ' . $home);
    }
    $items = new RecursiveIteratorIterator(
        new RecursiveDirectoryIterator($home, FilesystemIterator::SKIP_DOTS),
        RecursiveIteratorIterator::CHILD_FIRST
    );
    foreach ($items as $item) {
        $path = $item->getPathname();
        $removed = ($item->isDir() && !$item->isLink()) ? @rmdir($path) : @unlink($path);
        if (!$removed) {
            throw new RuntimeException('cannot remove CDNBest data: ' . $path);
        }
    }
    if (!@rmdir($home)) {
        throw new RuntimeException('cannot remove CDNBest directory: ' . $home);
    }
}

function uninstallCdnBest($kangleDirectory, $plan, &$warnings)
{
    $serviceFiles = $plan['service_files'];
    $systemctl = availableProgram(array('/bin/systemctl', '/usr/bin/systemctl'));
    $detail = '';
    if (count($serviceFiles) > 0) {
        $stopped = false;
        if ($systemctl !== null) {
            $stopped = runCdnBestCommand($systemctl, array('stop', 'cdnbest'), $detail);
        }
        if (!$stopped && is_executable('/etc/init.d/cdnbest')) {
            $stopped = runCdnBestCommand('/etc/init.d/cdnbest', array('stop'), $detail);
        }
        if (!$stopped && count(findCdnBestProcessIds($kangleDirectory)) > 0) {
            addWarning($warnings, 'CDNBest service stop failed; stopping its verified processes directly: ' . $detail);
        }
    }
    if (!waitForCdnBestProcesses($kangleDirectory)) {
        signalCdnBestProcesses($kangleDirectory, 15);
        if (!waitForCdnBestProcesses($kangleDirectory)) {
            signalCdnBestProcesses($kangleDirectory, 9);
            if (!waitForCdnBestProcesses($kangleDirectory)) {
                throw new RuntimeException('CDNBest processes are still running; installation files were not removed');
            }
        }
    }
    if (count($plan['process_ids']) > 0 || count($serviceFiles) > 0) {
        fwrite(STDOUT, "[STOP] cdnbest\n");
    }

    if (count($serviceFiles) > 0) {
        if ($systemctl !== null && !runCdnBestCommand($systemctl, array('disable', 'cdnbest'), $detail)) {
            addWarning($warnings, 'systemctl disable cdnbest failed; startup links will be removed directly: ' . $detail);
        }
        $chkconfig = availableProgram(array('/sbin/chkconfig', '/usr/sbin/chkconfig'));
        if ($chkconfig !== null) {
            runCdnBestCommand($chkconfig, array('--del', 'cdnbest'), $detail);
        }
    }

    $removedCount = 0;
    foreach (array_merge($plan['startup_links'], $serviceFiles) as $path) {
        if (!is_file($path) && !is_link($path)) {
            continue;
        }
        if (!@unlink($path)) {
            throw new RuntimeException('cannot remove CDNBest service or startup link: ' . $path);
        }
        ++$removedCount;
        fwrite(STDOUT, '[REMOVE] ' . $path . "\n");
    }
    if (count($serviceFiles) > 0 && $systemctl !== null &&
        !runCdnBestCommand($systemctl, array('daemon-reload'), $detail)) {
        addWarning($warnings, 'systemctl daemon-reload failed: ' . $detail);
    }
    if (count($serviceFiles) > 0 && $systemctl !== null) {
        runCdnBestCommand($systemctl, array('reset-failed', 'cdnbest'), $detail);
    }
    if ($plan['home'] !== null) {
        removeCdnHome($kangleDirectory, $plan['home']);
        ++$removedCount;
        fwrite(STDOUT, '[REMOVE] ' . $plan['home'] . " (directory)\n");
    }
    return $removedCount;
}

function serializeDocument($doc)
{
    $doc->encoding = 'UTF-8';
    $doc->formatOutput = true;
    $xml = $doc->saveXML();
    // Kangle extension files use a leading <!--#start N--> directive to
    // control load order. DOMDocument::saveXML() prepends an XML declaration,
    // which hides that directive from Kangle and makes the file fall back to
    // the default priority. Kangle configuration is UTF-8 without requiring a
    // declaration, so remove only the declaration generated at the beginning.
    $xml = preg_replace('/\A(?:\xEF\xBB\xBF)?\s*<\?xml\b[^>]*\?>\s*/i', '', $xml, 1);
    return rtrim($xml) . "\n";
}

function discoverAccessFiles($ftpRoot, &$warnings)
{
    $files = array();
    if (!is_dir($ftpRoot)) {
        addWarning($warnings, "site root does not exist and was skipped: {$ftpRoot}");
        return $files;
    }
    try {
        $groups = new DirectoryIterator($ftpRoot);
        foreach ($groups as $group) {
            if ($group->isDot() || $group->isLink() || !$group->isDir()) {
                continue;
            }
            $sites = new DirectoryIterator($group->getPathname());
            foreach ($sites as $site) {
                if ($site->isDot() || $site->isLink() || !$site->isDir()) {
                    continue;
                }
                $candidate = $site->getPathname() . '/access.xml';
                if (is_link($candidate)) {
                    addWarning($warnings, 'symbolic-link access.xml was skipped: ' . $candidate);
                    continue;
                }
                if (is_file($candidate)) {
                    $files[] = $candidate;
                }
            }
        }
    } catch (UnexpectedValueException $e) {
        throw new RuntimeException("cannot scan {$ftpRoot}: " . $e->getMessage());
    }
    sort($files, SORT_STRING);
    return $files;
}

function outputPathFor($source, $outputDirectory)
{
    $normalized = str_replace('\\', '/', $source);
    $normalized = preg_replace('/^[A-Za-z]:\//', '$0', $normalized);
    $normalized = ltrim($normalized, '/');
    return $outputDirectory . '/' . $normalized;
}

function formatStats($stats)
{
    if (count($stats) === 0) {
        return 'XML formatting/encoding normalization only';
    }
    $parts = array();
    ksort($stats);
    foreach ($stats as $name => $count) {
        $parts[] = $name . '=' . $count;
    }
    return implode(', ', $parts);
}

function prepareTemporaryFile($target, $content, $sourceForMetadata)
{
    $directory = dirname($target);
    if (!is_dir($directory) && !@mkdir($directory, 0755, true) && !is_dir($directory)) {
        throw new RuntimeException("cannot create output directory {$directory}");
    }
    $temporary = $target . '.migrate.' . getmypid() . '.' . mt_rand(1000, 9999);
    if (@file_put_contents($temporary, $content, LOCK_EX) === false) {
        throw new RuntimeException("cannot write temporary file {$temporary}");
    }
    $mode = @fileperms($sourceForMetadata);
    if ($mode !== false) {
        @chmod($temporary, $mode & 0777);
    } else {
        @chmod($temporary, 0644);
    }
    if (function_exists('fileowner')) {
        $owner = @fileowner($sourceForMetadata);
        $group = @filegroup($sourceForMetadata);
        if ($owner !== false) {
            @chown($temporary, $owner);
        }
        if ($group !== false) {
            @chgrp($temporary, $group);
        }
    }
    return $temporary;
}

if (defined('KANGLE_MIGRATION_LIBRARY_ONLY') && KANGLE_MIGRATION_LIBRARY_ONLY) {
    return;
}

$options = parseArguments($argv);
$globalWarnings = array();
$inputs = array();

if ($options['main']) {
    if (!is_file($options['config'])) {
        fwrite(STDERR, "error: main config does not exist: {$options['config']}\n");
        exit(1);
    }
    $inputs[$options['config']] = 'main';
}
foreach ($options['access_files'] as $file) {
    if (!is_file($file)) {
        fwrite(STDERR, "error: access file does not exist: {$file}\n");
        exit(1);
    }
    $inputs[$file] = 'access';
}
if ($options['sites']) {
    try {
        foreach (discoverAccessFiles($options['ftp_root'], $globalWarnings) as $file) {
            $inputs[$file] = 'access';
        }
    } catch (Exception $e) {
        fwrite(STDERR, 'error: ' . $e->getMessage() . "\n");
        exit(1);
    }
}

// Installation-wide maintenance belongs only to a real main-config migration.
// In particular, demo conversions using --output-dir must never modify or
// copy unrelated files from the active Kangle installation.
$maintenanceEnabled = $options['main'] && $options['output_dir'] === null;
$cleanupPaths = array();
$cdnBestPlan = null;
if ($maintenanceEnabled) {
    try {
        foreach (discoverPhpExtensionConfigs($options['kangle_dir'], $globalWarnings) as $file) {
            if (!isset($inputs[$file])) {
                $inputs[$file] = 'php';
            }
        }
        $cleanupPaths = discoverObsoleteInstallationFiles($options['kangle_dir'], $globalWarnings);
        $cdnBestPlan = discoverCdnBestRemoval($options['kangle_dir']);
    } catch (Exception $e) {
        fwrite(STDERR, 'error: ' . $e->getMessage() . "\n");
        exit(1);
    }
}

$templateDoc = null;
if (is_file($options['template'])) {
    try {
        $templateRaw = '';
        $templateWarnings = array();
        $templateDoc = loadXmlFile($options['template'], $templateRaw, $templateWarnings);
        foreach ($templateWarnings as $warning) {
            addWarning($globalWarnings, 'template: ' . $warning);
        }
    } catch (Exception $e) {
        fwrite(STDERR, 'error: cannot load template: ' . $e->getMessage() . "\n");
        exit(1);
    }
} else {
    addWarning($globalWarnings, 'config-default.xml was not found; built-in conservative defaults will be used');
}

// Convert access files first so the main config can add required DSO loaders.
$plans = array();
$allModules = array();
$ordered = array();
foreach ($inputs as $path => $type) {
    if ($type === 'access') {
        $ordered[$path] = $type;
    }
}
foreach ($inputs as $path => $type) {
    if ($type === 'main') {
        $ordered[$path] = $type;
    }
}
foreach ($inputs as $path => $type) {
    if ($type === 'php') {
        $ordered[$path] = $type;
    }
}

$hadError = false;
foreach ($ordered as $path => $type) {
    $warnings = array();
    $stats = array();
    try {
        $raw = '';
        $doc = loadXmlFile($path, $raw, $warnings);
        if ($type === 'access') {
            migrateAccessDocument($doc, $stats, $warnings, $allModules);
        } elseif ($type === 'php') {
            migratePhpExtensionDocument($doc, $stats, $warnings);
        } else {
            migrateMainDocument($doc, $templateDoc, $options, $allModules, $stats, $warnings);
        }
        $content = serializeDocument($doc);
        // Avoid rewriting every compatible access.xml merely because
        // DOMDocument normalizes indentation and quote style. Encoding
        // conversion is still a real change even without schema changes.
        $encodingChanged = in_array('input encoding was converted from GB18030/GBK to UTF-8', $warnings, true);
        $plans[] = array(
            'path' => $path,
            'type' => $type,
            'content' => $content,
            'changed' => (count($stats) > 0 || $encodingChanged),
            'stats' => $stats,
            'warnings' => $warnings,
        );
    } catch (Exception $e) {
        fwrite(STDERR, '[ERROR] ' . $e->getMessage() . "\n");
        $hadError = true;
    }
}
if ($hadError) {
    fwrite(STDERR, "No files were written because at least one input failed validation.\n");
    exit(1);
}

foreach ($globalWarnings as $warning) {
    fwrite(STDERR, '[WARN] ' . $warning . "\n");
}
$changedCount = 0;
foreach ($plans as $plan) {
    $state = $plan['changed'] ? 'CHANGE' : 'OK';
    if ($plan['changed']) {
        ++$changedCount;
    }
    fwrite(STDOUT, '[' . $state . '] ' . $plan['path'] . ' (' . $plan['type'] . ")\n");
    if ($plan['changed']) {
        fwrite(STDOUT, '         ' . formatStats($plan['stats']) . "\n");
    }
    foreach ($plan['warnings'] as $warning) {
        fwrite(STDERR, '[WARN] ' . $plan['path'] . ': ' . $warning . "\n");
    }
}
if ($options['dry_run']) {
    if ($cdnBestPlan !== null) {
        if (count($cdnBestPlan['service_files']) > 0 || count($cdnBestPlan['process_ids']) > 0) {
            fwrite(STDOUT, "[STOP] cdnbest\n");
        }
        if (count($cdnBestPlan['service_files']) > 0) {
            fwrite(STDOUT, "[DISABLE] cdnbest service\n");
        }
        foreach (array_merge($cdnBestPlan['startup_links'], $cdnBestPlan['service_files']) as $path) {
            fwrite(STDOUT, '[REMOVE] ' . $path . " (CDNBest service or startup link)\n");
        }
        if ($cdnBestPlan['home'] !== null) {
            fwrite(STDOUT, '[REMOVE] ' . $cdnBestPlan['home'] . " (CDNBest directory)\n");
        }
    }
    foreach ($cleanupPaths as $path) {
        fwrite(STDOUT, '[REMOVE] ' . $path . " (obsolete installation file)\n");
    }
    $cdnBestCount = $cdnBestPlan === null ? 0 :
        count($cdnBestPlan['startup_links']) + count($cdnBestPlan['service_files']) +
        ($cdnBestPlan['home'] === null ? 0 : 1);
    $operationCount = $changedCount + count($cleanupPaths) + $cdnBestCount;
    fwrite(STDOUT, "Dry run complete: {$operationCount} file operation(s) would run; nothing was changed.\n");
    exit(0);
}

$writePlans = array();
foreach ($plans as $plan) {
    if ($options['output_dir'] === null && !$plan['changed']) {
        continue;
    }
    $target = $options['output_dir'] === null ? $plan['path'] : outputPathFor($plan['path'], $options['output_dir']);
    $plan['target'] = $target;
    $writePlans[] = $plan;
}

$temporaries = array();
$removedCount = 0;
try {
    foreach ($writePlans as $index => $plan) {
        $temporaries[$index] = prepareTemporaryFile($plan['target'], $plan['content'], $plan['path']);
    }
    foreach ($writePlans as $index => $plan) {
        if (!@rename($temporaries[$index], $plan['target'])) {
            throw new RuntimeException("cannot replace {$plan['target']}");
        }
        unset($temporaries[$index]);
        fwrite(STDOUT, '[WRITE] ' . $plan['target'] . "\n");
    }
    if ($cdnBestPlan !== null) {
        $runtimeWarnings = array();
        $removedCount += uninstallCdnBest($options['kangle_dir'], $cdnBestPlan, $runtimeWarnings);
        foreach ($runtimeWarnings as $warning) {
            fwrite(STDERR, '[WARN] ' . $warning . "\n");
        }
    }
    foreach ($cleanupPaths as $path) {
        if (!@unlink($path)) {
            throw new RuntimeException("cannot remove obsolete file {$path}");
        }
        ++$removedCount;
        fwrite(STDOUT, '[REMOVE] ' . $path . "\n");
    }
} catch (Exception $e) {
    foreach ($temporaries as $temporary) {
        @unlink($temporary);
    }
    fwrite(STDERR, '[ERROR] ' . $e->getMessage() . "\n");
    fwrite(STDERR, "Migration stopped; completed writes and removals were not rolled back.\n");
    exit(1);
}

if ($options['reload']) {
    if ($options['output_dir'] !== null) {
        fwrite(STDERR, "[WARN] --reload was ignored because --output-dir does not update the live files.\n");
    } else {
        $binary = $options['kangle_dir'] . '/bin/kangle';
        if (!is_executable($binary)) {
            fwrite(STDERR, "[ERROR] cannot reload: {$binary} is not executable\n");
            exit(1);
        }
        $command = escapeshellarg($binary) . ' -r';
        passthru($command, $reloadStatus);
        if ($reloadStatus !== 0) {
            fwrite(STDERR, "[ERROR] Kangle reload failed with status {$reloadStatus}.\n");
            exit(1);
        }
        fwrite(STDOUT, "Kangle configuration reloaded successfully.\n");
    }
}

fwrite(STDOUT, 'Migration complete: ' . count($writePlans) . ' file(s) written, ' . $removedCount . " obsolete file(s) removed.\n");
exit(0);
