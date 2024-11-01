#!/usr/bin/env python3

import warnings
with warnings.catch_warnings():
    warnings.simplefilter('ignore')
    import colour

def spectrum_to_sRGB(spectrum):
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', category=colour.utilities.ColourRuntimeWarning)

        sd = colour.SpectralDistribution(spectrum)
        cmfs = colour.MSDS_CMFS["CIE 1931 2 Degree Standard Observer"]
        illuminant = colour.SDS_ILLUMINANTS["D65"]

        XYZ = colour.sd_to_XYZ(sd, cmfs, illuminant)
        sRGB = colour.XYZ_to_sRGB(XYZ / 100)

        return tuple(map(round, sRGB * 255))

if __name__ == '__main__':
    pass
