#credits: https://gist.github.com/Soarez/9688998
#         https://www.dogtagpki.org/wiki/Creating_Self-Signed_CA_Signing_Certificate_with_OpenSSL

script_dir="$( cd "$( dirname "${BASH_SOURCE[0]}" )" >/dev/null 2>&1 && pwd )"

function die() {
    [ "$#" -gt 0 ] && echo $* >&2
    exit 1;
}

cd $script_dir || die "failed to cd to $script_dir"
(! [ -d ssl_dir ] || rm -rf ssl_dir) && mkdir ssl_dir || die "failed to create ssl_dir"
cd ssl_dir || die "failed to cd to $script_dir/ssl_dir"


#Generating a CA:
openssl genrsa -out ca.key 2048 || die "failed to generate ca"
openssl req -new -nodes -x509 -key ca.key -out ca.crt -days 60 -config <(cat <<EOF
[ ca ]
default_ca = CA_default
[ CA_default ]
default_days    = 1000          # how long to certify for
default_crl_days = 30           # how long before next CRL
default_md  = sha256            # use public key default MD
preserve    = no                # keep passed DN ordering

x509_extensions = ca_extensions

email_in_dn = no                # Don't concat the email in the DN
copy_extensions = copy          # Required to copy SANs from CSR to cert

[req]
distinguished_name = req_distinguished_name
default_bits = 4096
encrypt_key = no
x509_extensions     = ca_extensions
prompt = no
utf8 = yes
[req_distinguished_name]
countryName                    = SA

stateOrProvinceName            = Riyadh

localityName                   = Riyadh

organizationName               = Peanuts Inc.

organizationalUnitName         = Peanuts Quality Assurance division

commonName                     = Super Ca

emailAddress                   = peanuts@nilput.com
[ ca_extensions ]
subjectKeyIdentifier = hash
authorityKeyIdentifier = keyid:always, issuer
basicConstraints = critical, CA:true
keyUsage = critical, digitalSignature, nonRepudiation, keyCertSign, cRLSign
EOF
) || die "failed to generate ca crt"


for domain_name in site_a.local site_b.local; do

    openssl genrsa -out "$domain_name.privkey.pem" 2048 || die "failed to $domain_name privkey"
    cat > "$domain_name.ext" << EOF
authorityKeyIdentifier=keyid,issuer
basicConstraints=CA:FALSE
keyUsage = digitalSignature, nonRepudiation, keyEncipherment, dataEncipherment
subjectAltName = @alt_names
[alt_names]
DNS.1 = www.$domain_name
DNS.2 = $domain_name
EOF
    openssl req -new -out "$domain_name.csr" -config <(cat <<EOF
[ req ]
default_bits = 2048
default_keyfile = $domain_name.privkey.pem
encrypt_key = no
default_md = sha1
prompt = no
utf8 = yes
distinguished_name = my_req_distinguished_name

[ my_req_distinguished_name ]
C = PT
ST = Lisboa
L = Lisboa
O  = Oats In The Water
CN = $domain_name

EOF
) || die "failed to generate $domain_name csr"


    #to inspect csr:
    #openssl req -in site_a.csr -noout -text


    openssl x509 -req -in "$domain_name.csr" -CA ca.crt -CAkey ca.key -days 60 -CAcreateserial -out "$domain_name.crt" -extfile "$domain_name.ext" || die "failed to sign $domain_name csr"

    cat "$domain_name.crt" ca.crt > "$domain_name.fullchain.pem"

    #to inspect signed certificate
    #openssl x509 -in example.org.crt -noout -text

done
