# Real-time Apache access log splitter

Split the Apache access log in multi-domain hosting configurations into
individual 2nd-level domain access logs in real-time, including splitting
according to years and months.

Use with the following Apache configuration directives:

```
LogFormat "%V %h %l %u %t \"%r\" %>s %b \"%{Referer}i\" \"%{User-agent}i\"" combined
CustomLog "|/usr/local/sbin/accesslog" combined
```

The destination prefix is currently hardwired to `/home/httpd`. The optional
argument can be used to add a prefix to the target log file name (e.g. for SSL).
