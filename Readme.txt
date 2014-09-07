1. To compile project type make command and it will generate a myhttpd object file
2. to run the server type ./myhttpd and give required options to it.  For ex:-/.myhttpd -p 2000 -t 30 -l mylog.txt

	
3. When using telnet please give requests in the format as follows 
   GET <path> HTTP/1.0
   or 
   HEAD <path> HTTP/1.0
   
   Please note that the 3rd parameter specifying the HTTP version is not optional. Also please start pathname using a "/"

4. On browser the following mimeTypes will be supported
  
     text/html"
     "text/xml"
     "text/plain"
     "text/css", 
     "image/png", 
     "image/gif", 
     "image/jpg", 
     "image/jpeg", 
     "application/zip
    
 5. Most web pages with images and links will be rendered correctly if they are having content restricted to the abouve mime types. Browsers on which myhttpd  server was tested is Google Chrome Version 29.0.1547.62 and Mozilla Firefox for ubuntu 23.0


. 

Note:- When both -d and -l are specified myhttpd will write into a log file as well as write to stdout.
