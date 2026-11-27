<?php
define('KANGLE_MIGRATION_LIBRARY_ONLY', true);
require dirname(__DIR__) . '/tools/migrate-config.php';

$checks = 0;
function checkMigration($condition, $label)
{
    global $checks;
    if (!$condition) {
        fwrite(STDERR, 'FAIL: ' . $label . "\n");
        exit(1);
    }
    ++$checks;
    echo 'PASS: ' . $label . "\n";
}

$doc = new DOMDocument();
$doc->loadXML('<config><vhs><mime_type ext="txt" type="text/plain"/><vh name="first"><bind>!*:3312</bind></vh><vh name="second"><bind>!*:3313</bind></vh></vhs></config>');
$stats = array(); $warnings = array(); $modules = array();
migrateAccessDocument($doc, $stats, $warnings, $modules);
$xpath = new DOMXPath($doc);
checkMigration($xpath->query('/config/vh')->length === 2, 'both legacy virtual hosts become root declarations');
checkMigration($xpath->query('/config/vhs/vh')->length === 0, 'no ignored nested virtual hosts remain');
checkMigration($xpath->query('/config/vhs/mime_type')->length === 1, 'global virtual-host defaults are preserved');
checkMigration($xpath->evaluate('string(/config/vh[@name="first"]/bind)') === '!*:3312', 'listener binding is preserved');
$before = $doc->saveXML();
$stats = array();
migrateAccessDocument($doc, $stats, $warnings, $modules);
checkMigration($doc->saveXML() === $before && count($stats) === 0, 'repeated conversion is idempotent');

$doc = new DOMDocument();
$doc->loadXML('<config><vhs><vh name="only"/></vhs></config>');
$stats = array();
migrateAccessDocument($doc, $stats, $warnings, $modules);
$xpath = new DOMXPath($doc);
checkMigration($xpath->query('/config/vh')->length === 1 && $xpath->query('/config/vhs')->length === 0,
    'an empty legacy container is removed after lifting its host');

echo 'SUCCESS: ' . $checks . " migration checks\n";
