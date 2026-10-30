# Source and arithmetic audit only. Does not compile or execute Unreal automation tests.
$ErrorActionPreference = 'Stop'
$combat = Join-Path $PSScriptRoot '../AI/Combat'
$files = @('DMCombatHealthEvaluation.h','DMCombatHealthEvaluationTests.cpp','DMCombatModules.h',
 'DMCombatTacticsComponent.h','DMCombatTacticsComponent.cpp','DMCombatPositionRisk.h','DMCombatPositionRisk.cpp',
 'DMCombatMovementDecision.cpp','DMCombatAttackPosition.h','DMCombatAttackPosition.cpp',
 'DMCombatCoverModule.h','DMCombatCoverModule.cpp','DMCombatThreatMemory.cpp','DMCombatTacticsDebug.cpp')
$script:checks = 0
function Check($ok, $why) { if (!$ok) { throw $why }; $script:checks++ }
$texts = @{}
foreach ($file in $files) {
 $s = [IO.File]::ReadAllText((Join-Path $combat $file))
 $texts[$file] = $s
 Check ($s -notmatch '(?m)^(<<<<<<<|=======|>>>>>>>)') "$file conflict marker"
 $code = [regex]::Replace($s, '(?s)/\*.*?\*/|//[^\r\n]*|"(?:\\.|[^"\\])*"|''(?:\\.|[^''\\])*''', '')
 $stack = [Collections.Generic.Stack[char]]::new()
 $pairs = @{ ')'='('; ']'='['; '}'='{' }
 foreach ($char in $code.ToCharArray()) {
  if ('([{'.Contains([string]$char)) { $stack.Push($char) }
  elseif ($pairs.ContainsKey([string]$char)) {
   if ($stack.Count -eq 0 -or $stack.Pop() -ne $pairs[[string]$char]) { throw "$file delimiter mismatch" }
  }
 }
 Check ($stack.Count -eq 0) "$file unclosed delimiter"
 $includes = @([regex]::Matches($s, '(?m)^#include[^\r\n]*') | ForEach-Object Value)
 if ($s.Contains('.generated.h')) { Check ($includes[-1] -match '\.generated\.h') "$file generated include must be last" }
}
$production = ($texts.GetEnumerator() | Where-Object Key -ne 'DMCombatHealthEvaluationTests.cpp' | ForEach-Object Value) -join "`n"
$health = $texts['DMCombatHealthEvaluation.h']
$settings = [regex]::Matches($health, 'UPROPERTY\([^\r\n]*\) (?:float|bool) (\w+)\s*=')
Check ($settings.Count -eq 11) 'Expected 11 health settings'
foreach ($match in $settings) {
 $name = $match.Groups[1].Value
 Check ($production -match ('\b(?:S|HealthEvaluation)\.' + $name + '\b')) "$name unused production setting"
 Check ($match.Value.Contains('ToolTip=')) "$name missing tooltip"
}
Check (([regex]::Matches($texts['DMCombatTacticsComponent.cpp'],'DMCombatHealth::Evaluate\(')).Count -eq 1) 'Health evaluated once in BuildContext'
Check ($texts['DMCombatMovementDecision.cpp'] -notmatch 'Get(?:Max)?Health\(') 'Movement must not read health API'
Check ($texts['DMCombatPositionRisk.cpp'].IndexOf('CachePositionRisk(C,G,S,bPath,R)') -lt $texts['DMCombatPositionRisk.cpp'].IndexOf('R.HealthRatio=C.Health.HealthRatio')) 'Geometry cached before health cost'
Check (([regex]::Matches($texts['DMCombatPositionRisk.cpp'],'R.Total\+=DMCombatHealth::ExposureCost')).Count -eq 1) 'Exactly one health addition'
Check ($texts['DMCombatPositionRisk.cpp'] -match 'R.WeightedPathExposure\+=SampleExposure/Samples') 'Path exposure remains independent of legacy path weight'
Check ($texts['DMCombatMovementDecision.cpp'] -match 'DMCombatHealth::Emergency\(C.Health,bLowHealthExposureTriggersEmergency') 'Legacy emergency consumes shared ratio'
Check ($texts['DMCombatAttackPosition.cpp'] -match 'S.TotalScore\+Risk.Total-S.HealthCoverReward\+S.HealthAggressionPenalty') 'Base score threshold retains old semantics'
$debugBlock = ($texts['DMCombatTacticsDebug.cpp'] -split 'if \(C\)\s*\{')[-1] -split 'const FVector Thresholds' | Select-Object -First 1
Check ($debugBlock -notmatch 'DMCombatHealth::Evaluate|DMPositionRisk::Evaluate|FRand|RandBool|BuildContext\(') 'New debug output must not evaluate behavior'
# Independently calculate documented formulas; these are not C++ test results.
function Formula($current, $maximum, $weight=1.0, $enabled=$true, $caution=.65, $critical=.30) {
 $ratio=1.0
 if ([double]::IsFinite($maximum) -and $maximum -gt 0 -and [double]::IsFinite($current)) { $ratio=[Math]::Clamp($current/$maximum,0.0,1.0) }
 $risk=1-$ratio
 $weighted=if ($enabled) { $risk*[Math]::Max(0,$weight) } else { 0 }
 $hi=[Math]::Max([Math]::Clamp($caution,0.0,1.0),[Math]::Clamp($critical,0.0,1.0))
 $lo=[Math]::Min([Math]::Clamp($caution,0.0,1.0),[Math]::Clamp($critical,0.0,1.0))
 $preference=$weighted*[Math]::Clamp(($hi-$ratio)/[Math]::Max(1e-8,$hi-$lo),0.0,1.0)
 return @{ Ratio=$ratio; Risk=$risk; Exposure=20*$weighted; Path=10*$weighted; Micro=1-.5*[Math]::Clamp($weighted,0.0,1.0); Cover=20*$preference }
}
foreach ($hp in @(100,50,10)) {
 $h=Formula $hp 100
 Check ([Math]::Abs($h.Risk-(1-$hp/100.0)) -lt 1e-6) "$hp risk formula"
 Check ([Math]::Abs(($h.Exposure+$h.Path)-30*(1-$hp/100.0)) -lt 1e-6) "$hp exposure formula"
 Write-Output "HP $hp/100: ratio=$($h.Ratio) risk=$($h.Risk) destination=$($h.Exposure) path=$($h.Path) micro=$($h.Micro)"
}
foreach ($max in @(0,-1,[double]::NaN,[double]::PositiveInfinity)) { Check ((Formula 10 $max).Ratio -eq 1) 'Invalid maximum' }
Check ((Formula -10 100).Ratio -eq 0) 'Negative current clamp'
Check ((Formula 200 100).Ratio -eq 1) 'Overheal clamp'
Check ((Formula 10 100 0).Exposure -eq 0) 'Zero weight'
Check ((Formula 10 100 1 $false).Micro -eq 1) 'Disabled micro'
Check ((Formula 50 100 1 $true .30 .65).Cover -eq (Formula 50 100).Cover) 'Reversed bands'
Check ([Math]::Abs((Formula 50 100).Micro-.75) -lt 1e-6) 'Half micro radius'
Check ([Math]::Abs((Formula 10 100).Micro-.55) -lt 1e-6) 'Low micro radius'
Write-Output "PASS: $script:checks source/arithmetic checks. C++/UHT/Unreal tests NOT run."
