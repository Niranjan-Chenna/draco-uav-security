#include "mission_revision.h"

#include <iomanip>
#include <sstream>
#include <stdexcept>

#include <openssl/evp.h>

namespace {

std::string bytes_to_hex(
    const unsigned char* bytes,
    unsigned int length
) {
    std::ostringstream output;

    output << std::hex
           << std::setfill('0');

    for (unsigned int i = 0; i < length; ++i) {
        output << std::setw(2)
               << static_cast<unsigned int>(
                      bytes[i]
                  );
    }

    return output.str();
}

}

std::string compute_mission_hash(
    const CanonicalMission& mission
) {
    const std::string serialized =
        serialize_canonical_mission(
            mission
        );

    EVP_MD_CTX* context =
        EVP_MD_CTX_new();

    if (context == nullptr) {
        throw std::runtime_error(
            "failed to create hash context"
        );
    }

    unsigned char digest[
        EVP_MAX_MD_SIZE
    ];

    unsigned int digest_length = 0;

    bool success =
        EVP_DigestInit_ex(
            context,
            EVP_sha256(),
            nullptr
        ) == 1
        &&
        EVP_DigestUpdate(
            context,
            serialized.data(),
            serialized.size()
        ) == 1
        &&
        EVP_DigestFinal_ex(
            context,
            digest,
            &digest_length
        ) == 1;

    EVP_MD_CTX_free(context);

    if (!success) {
        throw std::runtime_error(
            "failed to compute mission hash"
        );
    }

    return bytes_to_hex(
        digest,
        digest_length
    );
}

MissionRevision make_mission_revision(
    MissionRevisionId id,
    const CanonicalMission& mission
) {
    MissionRevision revision{};

    revision.id = id;
    revision.hash =
        compute_mission_hash(mission);
    revision.mission = mission;

    return revision;
}