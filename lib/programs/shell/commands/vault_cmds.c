#include "../commands.h"
#include "vault.h"

//////////////////////////////////
// SETUP:

bool cmd_vault_setup_help(const out_t *out) {
    out->write(out->ctx, "sets up /VAULT and SHN.VLT for Vault function");
    return true;
}

bool cmd_vault_setup_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_vault_setup_help(out);
}

bool cmd_vault_setup(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_vault_setup_help(out);
    }

    if (argc != 1) {
        return cmd_vault_setup_fail(out);
    }

    // this returns true or false on success
    //  but it manages its own messaging
    vault_setup(out);

    return true;
}



//////////////////////////////////
// ENCRYPT:

bool cmd_vault_encrypt_help(const out_t *out) {
    out->write(out->ctx, "Uses SHN.VLT to encrypt the VAULT/ directory");
    return true;
}

bool cmd_vault_encrypt_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_vault_encrypt_help(out);
}

bool cmd_vault_encrypt(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_vault_encrypt_help(out);
    }

    if (argc != 1) {
        return cmd_vault_encrypt_fail(out);
    }

    // this returns true or false on success
    //  but it manages its own messaging
    vault_encrypt(out);

    return true;
}

//////////////////////////////////
// DECRYPT:

bool cmd_vault_decrypt_help(const out_t *out) {
    out->write(out->ctx, "Uses SHN.VLT to decrypt the VAULT/ directory");
    return true;
}

bool cmd_vault_decrypt_fail(const out_t *out) {
    out->write(out->ctx, "CMD FAIL:");
    return cmd_vault_decrypt_help(out);
}

bool cmd_vault_decrypt(uint8_t argc, const char ** argv, const out_t *out) {

    if (argc >= 2 && ((strcmp(argv[1], "-h") == 0) || (strcmp(argv[1], "--help") == 0) || (strcmp(argv[1], "help") == 0))) {
        return cmd_vault_decrypt_help(out);
    }

    if (argc != 1) {
        return cmd_vault_decrypt_fail(out);
    }

    // this returns true or false on success
    //  but it manages its own messaging
    vault_decrypt(out);

    return true;
}