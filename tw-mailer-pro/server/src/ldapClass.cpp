#include "ldapClass.h"

#include <ldap.h>
#include <stdexcept>

#include "ldapUtils.h"
#include "logger.h"

Ldap::Ldap(std::string uri, std::string base, ber_int_t scope, std::string filter) {
    this->uri = uri;
    this->base = base;
    this->scope = scope;
    this->filter = filter;
}

void Ldap::connect() {
    Logger::info("Connecting to LDAP server...");
    if (ldap_initialize(&ldap, uri.c_str()) != LDAP_SUCCESS) {
        std::string error = "Could not connect to LDAP server";
        Logger::error(error);
        throw std::runtime_error(error);
    }

    Logger::success("Connected to LDAP server");
}

void Ldap::setProtocolVersion(int ldapVersion) {
    Logger::info("Setting LDAP protocol version...");
    int rc;
    if ((rc = ldap_set_option(ldap, LDAP_OPT_PROTOCOL_VERSION, &ldapVersion)) != LDAP_SUCCESS) {
        ldap_unbind_ext_s(ldap, NULL, NULL);
        Logger::error(ldap_err2string(rc));
        throw std::runtime_error("Could not set LDAP protocol version");
    }

    Logger::success("Set LDAP protocol version");
}

void Ldap::startTls() {
    Logger::info("Starting TLS...");
    int rc;
    if ((rc = ldap_start_tls_s(ldap, NULL, NULL)) != LDAP_SUCCESS) {
        ldap_unbind_ext_s(ldap, NULL, NULL);
        Logger::error(ldap_err2string(rc));
        throw std::runtime_error("Could not start TLS");
    }

    Logger::success("Started TLS");
}

bool Ldap::bind(std::string username, std::string usernameSuffix, std::string password) {
    Logger::info("Binding " + username + " to LDAP server...");

    BerValue cred;
    cred.bv_val = (char*) password.c_str();
    cred.bv_len = password.length();

    BerValue* servercredp;
    return ldap_sasl_bind_s(ldap, ("uid=" + username + usernameSuffix).c_str(), LDAP_SASL_SIMPLE, &cred, NULL, NULL, &servercredp) == LDAP_SUCCESS;
}
