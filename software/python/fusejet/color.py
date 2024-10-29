#!/usr/bin/env python3

import warnings
with warnings.catch_warnings():
    warnings.simplefilter('ignore')
    import colour

def sensor_data_to_sRGB(data):
    with warnings.catch_warnings():
        warnings.simplefilter('ignore', category=colour.utilities.ColourRuntimeWarning)

        sd = colour.SpectralDistribution(data)
        cmfs = colour.MSDS_CMFS["CIE 1931 2 Degree Standard Observer"]
        illuminant = colour.SDS_ILLUMINANTS["D65"]

        XYZ = colour.sd_to_XYZ(sd, cmfs, illuminant)
        sRGB = colour.XYZ_to_sRGB(XYZ / 100)

        return tuple(map(round, sRGB * 255))

if __name__ == '__main__':
    data = {
        450: 0,
        500: 0,
        550: 0,
        570: 0.05,
        600: 0.1,
        650: 0.05,
    }
    print(sensor_data_to_sRGB(data))
