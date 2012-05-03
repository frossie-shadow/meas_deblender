import os
import math
import lsst.pipe.base as pipeBase
import lsst.pipe.tasks.processCcd as procCcd
import lsst.daf.persistence as dafPersist
import lsst.obs.suprimecam as obsSc

import lsst.daf.base as dafBase
import lsst.afw.table as afwTable
import lsst.afw.math  as afwMath
import lsst.afw.image as afwImage
from lsst.ip.isr import IsrTask
from lsst.pipe.tasks.calibrate import CalibrateTask

def printConfig(conf):
    for k,v in conf.items():
        print '  ', k, v

if __name__ == '__main__':
    basedir = os.path.join(os.environ['HOME'], 'lsst', 'ACT-data')
    mapperArgs = dict(root=os.path.join(basedir, 'rerun/dstn'),
                      calibRoot=os.path.join(basedir, 'CALIB'))
    mapper = obsSc.SuprimecamMapper(**mapperArgs)
    butlerFactory = dafPersist.ButlerFactory(mapper = mapper)
    butler = butlerFactory.create()
    print 'Butler', butler
    dataRef = butler.subset('raw', dataId = dict(visit=126969, ccd=5))
    print 'dataRef:', dataRef
    #dataRef.butlerSubset = dataRef
    #print 'dataRef:', dataRef
    print 'len(dataRef):', len(dataRef)
    for dr in dataRef:
        print '  ', dr

            
    conf = procCcd.ProcessCcdConfig()

    #conf.doDetection = True
    #conf.doMeasurement = True
    #conf.doWriteSources = True
    #conf.doWriteCalibrate = True

    conf.calibrate.doComputeApCorr = False
    conf.calibrate.doAstrometry = False
    conf.calibrate.doPhotoCal = False

    # the default Cosmic Ray parameters don't work for Subaru images
    cr = conf.calibrate.repair.cosmicray
    cr.minSigma = 10.
    cr.min_DN = 500.
    cr.niteration = 3
    cr.nCrPixelMax = 1000000

    proc = procCcd.ProcessCcdTask(config=conf, name='ProcessCcd')

    conf.calibrate.measurement.doApplyApCorr = False
    conf.measurement.doApplyApCorr = False
    conf.validate()

    for dr in dataRef:
        print 'dr', dr
        # Only do ISR, Calibration, and Measurement if necessary...
        doIsr    = False
        doCalib  = False
        doDetect = False
        print 'calexp', mapper.map('calexp', dr.dataId)
        print 'psf', mapper.map('psf', dr.dataId)
        print 'src', mapper.map('src', dr.dataId)
        try:
            psf = dr.get('psf')
            print 'PSF:', psf
        except:
            print 'No PSF'
            doCalib = True
        try:
            expo = dr.get('calexp')
            print 'Calexp:', expo
        except:
            print 'No calexp'
            doCalib = True
        # "icSrc" are created during 'calibrate'; only bright guys
        # "src"   are created during 'detection'
        # (then are passed to measurement to get filled in)
        try:
            srcs = dr.get('src')
            print 'Srcs', srcs
        except:
            print 'No sources'
            doDetect = True

        conf.doIsr            = doIsr
        conf.doCalibrate      = doCalib
        conf.doWriteCalibrate = doCalib
        conf.doDetection      = doDetect
        conf.doWriteSources   = doDetect

        conf.doMeasurement = False

        proc.run(dr)

        # Now we deblend and run measurement on the deblended hierarchy.
        srcs = dr.get('src')
        print 'Srcs', srcs

        # Make sure the IdFactory exists and doesn't duplicate IDs
        # (until Jim finishes #2083)
        f = srcs.getTable().getIdFactory()
        if f is None:
            f = afwTable.IdFactory.makeSimple()
            srcs.getTable().setIdFactory(f)
        f.notify(max([src.getId() for src in srcs]))
        f = srcs.getTable().getIdFactory()

        exposure = dr.get('calexp')
        print 'Exposure', exposure
        mi = exposure.getMaskedImage()

        psf = dr.get('psf')
        print 'PSF:', psf

        from lsst.meas.deblender.baseline import deblend
        import lsst.meas.algorithms as measAlg

        # find the median stdev in the image...
        stats = afwMath.makeStatistics(mi.getVariance(), mi.getMask(), afwMath.MEDIAN)
        sigma1 = math.sqrt(stats.getValue(afwMath.MEDIAN))

        ### HACK
        #srcs = srcs[:20]

        schema = srcs.getSchema()
        xkey = schema.find('centroid.naive.x').key
        ykey = schema.find('centroid.naive.y').key

        heavies = {}
        n0 = len(srcs)
        for src in srcs:
            # SourceRecords
            #print '  ', src
            fp = src.getFootprint()
            #print '  fp', fp
            pks = fp.getPeaks()
            #print '  pks:', len(pks), pks
            if len(pks) < 2:
                continue

            bb = fp.getBBox()
            xc = int((bb.getMinX() + bb.getMaxX()) / 2.)
            yc = int((bb.getMinY() + bb.getMaxY()) / 2.)

            if hasattr(psf, 'getFwhm'):
                psf_fwhm = psf.getFwhm(xc, yc)
            else:
                pa = measAlg.PsfAttributes(psf, xc, yc)
                psfw = pa.computeGaussianWidth(measAlg.PsfAttributes.ADAPTIVE_MOMENT)
                #print 'PSF width:', psfw
                psf_fwhm = 2.35 * psfw

            print 'Deblending', len(pks), 'peaks'
            X = deblend([fp], [pks], mi, psf, psf_fwhm, sigma1=sigma1)
            res = X[0]
            for pkres in res.peaks:
                child = srcs.addNew()
                child.setParent(src.getId())
                child.setFootprint(pkres.heavy)
                # == pk.getF{xy}(), for now
                x,y = pkres.center
                child.set(xkey, x)
                child.set(ykey, y)
                heavies[child.getId()] = pkres.heavy
        n1 = len(srcs)

        print 'Deblending:', n0, 'sources ->', n1

        print 'Measuring...'
        conf.measurement.doRemoveOtherSources = True
        proc.measurement.run(exposure, srcs)
        print 'Writing FITS...'
        srcs.writeFits('deblended-srcs.fits')

        # Write heavy footprints as well.
        keys = heavies.keys()
        keys.sort()
        for k in keys:
            h = heavies[k]
            bb = h.getBBox()
            mim = afwImage.MaskedImageF(bb.getWidth(), bb.getHeight())
            mim.setXY0(bb.getMinX(), bb.getMinY())
            h.insert(mim)
            fn = 'deblend-heavy-%04i.fits' % k
            mim.writeFits(fn)
            print 'Wrote', fn


