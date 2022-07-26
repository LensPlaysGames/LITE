program lite_test_runner;

{$mode objfpc}{$H+}

uses
Classes,
Math,
Process,
SysUtils;

// TODO: 'Quiet' variable based on command line flag to hide output
//       of non-failing tests.
// TODO: 'Verbose' variable based on cmd line flag to print each tests'
//       output, expected output, etc.
var
   LITERoot : string;
   LITEExe  : string;

function ArrayOfCharToString(characters : array of char; len: integer): string;
var
   i : integer;
begin
   ArrayOfCharToString := '';
   for i := 0 to len - 1 do
      ArrayOfCharToString := ArrayOfCharToString + characters[i];
end;

// Return zero upon successful test.
function run_test(path: string): integer;
var
   proc       : TProcess;
   out        : array [0..511] of char;
   outstring  : string;
   read_count : integer;
   expected   : string;
   TmpString  : string;
   infile     : TextFile;
begin
   //writeln('RUNNING TEST: ', path);
   run_test := 1;
   expected := '';
   AssignFile(infile, path);
   Reset(infile);
   while not eof(infile) do begin
      readln(infile, tmpstring);
      if Length(tmpstring) <= 2 then break;
      if tmpstring[1] <> ';' then break;
      Delete(tmpstring, 1, 2);
      expected := expected + tmpstring + LineEnding;
   end;

   // Test must expect some output (otherwise not much of a test, right?).
   if expected = '' then begin
      writeln('Test must expect output. Read the README for help.');
      exit(1);
   end;

   proc := TProcess.Create(nil);
   try
      proc.Options := proc.Options + [poUsePipes,poWaitOnExit];
      proc.Executable := LITEExe;
      proc.Parameters.Add('--script');
      proc.Parameters.Add(path);
      proc.CurrentDirectory := LITERoot;
      proc.Execute;
      run_test := 0;
      read_count := Min(512, proc.Output.NumBytesAvailable);
      proc.Output.Read(out, read_count);
      outstring := ArrayOfCharToString(out, Max(read_count, Min(512, Length(expected))));
      if outstring <> expected then begin
         writeln('  Output:   ', outstring);
         writeln('  Expected: ', expected);
         run_test := 1;
      end;
   finally
      proc.Free;
   end;
end;

procedure run_tests(directory : string);
var
   i           : integer;
   status      : integer;
   TestCount   : integer;
   PassedTests : integer;
   TestPath    : string;
   FailedTests : TStringList;
   DirEnt      : TSearchRec;
begin;
   writeln('Running tests in ', directory);
   TestCount := 0;
   PassedTests := 0;
   FailedTests := TStringList.Create;
   if FindFirst(directory + DirectorySeparator + '*.lt', 0, dirent) = 0 then begin
      repeat
         With dirent do begin
            inc(TestCount);
            TestPath := directory + DirectorySeparator + name;
            status := run_test(TestPath);
            if status <> 0 then begin
               FailedTests.Add(TestPath);
               writeln('FAIL: ', TestPath);
            end else begin
               inc(PassedTests);
               writeln('PASS: ', TestPath);
            end;
         end;
      until FindNext(dirent) <> 0;
      FindClose(dirent);
   end;
   writeln('');

   if FindFirst(directory + DirectorySeparator + '*_tests', faDirectory, dirent) = 0 then begin
      repeat
         With dirent do begin
            if Attr and faDirectory > 0 then begin
               run_tests(directory + DirectorySeparator + name);
               writeln('');
            end;
         end;
      until FindNext(dirent) <> 0;
      FindClose(dirent);
   end;

   if testcount = 0 then begin
      writeln('NO TESTS RUN IN     ' + directory);
   end else if passedtests = testcount then begin
      writeln('ALL TESTS PASSED IN ' + directory);
   end else if PassedTests > 0 then
      writeln(passedtests, ' PASSED TESTS');

   if FailedTests.count > 0 then begin
      writeln(failedtests.count, ' FAILED TESTS');
      for i := 0 to FailedTests.count - 1 do
         writeln('  ', failedtests[i]);
   end;

   FailedTests.Destroy;
end;

// MAIN PROGRAM ENTRY
begin
   GetDir(0, LITERoot);
   LITEExe := LITERoot + DirectorySeparator + 'bin' + DirectorySeparator + 'LITE';
   run_tests('tst');
end.
