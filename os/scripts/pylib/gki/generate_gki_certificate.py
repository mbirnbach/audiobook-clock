# Ubuntu's mkbootimg package imports this module but doesn't ship it.
# It's only used to sign boot image v4 (GKI); we build v2 images.
def generate_gki_certificate(*args, **kwargs):
    raise NotImplementedError("GKI boot certificates are not supported here")
