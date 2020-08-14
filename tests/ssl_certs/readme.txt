the following domains are used for testing vhosts and SSL:
    site_a.local
    site_b.local
they need the following host entries:
    127.0.0.1 site_a.local
    127.0.0.1 site_b.local

for SSL testing, running the script ssl.sh creates a fake root CA, in addition to certificates for the above domains, and saves them in a new directory named "ssl_dir", the root CA must be installed temporarily while testing in a browser or whatever, and don't forget to delete it later
