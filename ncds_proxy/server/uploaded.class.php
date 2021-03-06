<?php
error_reporting(E_ALL);
class Uploaded {
    private $tmppath = '';
    public function __construct($tmppath) {
        $this->tmppath = $tmppath;
    }

    // invoke CLIENT to upload the file to NCDS
    public function uploadToNCDS ($path) {

        print "Path: $path <br />";

        // CLIENT parameters
        $client_no = rand();
        $scheme = 'evenodd';
        $n = '7';

        // not used
        $k = '6';
        $m = '2';
        $w = '8';

        // log file
        $logfile = uniqid(rand(), true) . '.log';

        system ("cd ../bin; ./CLIENT_p -i $client_no -a upload -c $scheme -n $n -k $k -m $m -w $w -t $this->tmppath > $logfile 2>&1");

        $log_content = file_get_contents("../bin/$logfile", true);

        // check successful
        print "File ID: <br />";
        preg_match_all('/Upload .+ Done \[(.+)\]/', $log_content, $matches);
        $fileid = $matches[1][0];
        print $fileid;

        print "<br />";

        print "Last Successful Upload: <br />";
        preg_match_all("/.+transferred in.+/", $log_content, $matches);
        print $matches[0][0];

        print "<br />";

        unlink("../bin/$logfile");

        $this->path = $path;
        $this->fileid = $fileid;
        return $fileid;
    }

    public function makeBackup ($path, $backup_location) {
        system ("scp $this->tmppath $backup_location/$path");
        return true;
    }

    public function saveMappingToDb () {
        try{
            $db = new PDO('sqlite:filelist.sqlite3');
            $db->setAttribute(PDO::ATTR_ERRMODE, PDO::ERRMODE_EXCEPTION);
            $db->exec("DELETE FROM FILELIST WHERE PATH = '$this->path';");
            $db->exec("INSERT INTO FILELIST (PATH, FILEID) VALUES ('$this->path', $this->fileid);");
            print 'Saved to DB <br />';
            $db = null;
        } catch(PDOException $e) {
            print 'Exception : '.$e->getMessage();
        }

    }

}
?>
