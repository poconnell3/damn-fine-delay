# 05: Sources

## Primary: hardware and inventors

- **Putnam, M. T. & Cooper, D. H., US Patent 3,789,328, "Acoustical Delay Line"** (filed 1972-06-22, issued 1974-01-29). Covers tube lengths, the T-block cavity, the quarter-wave stub, Helmholtz tuning slugs, and the 3 kHz compensator. https://patents.google.com/patent/US3789328 · PDF: https://patentimages.storage.googleapis.com/f4/e3/40/7b4bf491ef571e/US3789328.pdf
- **Cooper, D. H., "Construction of a Madsen-System Delay Tube," *Audio*, Apr 1971 p.18 (Part I) and May 1971 p.48 (Part II).** Gives the tube loss formula, transducers, equalization, and the nonlinear distortion estimate. Part I: https://www.worldradiohistory.com/Archive-All-Audio/Archive-Audio/70s/Audio-1971-04.pdf (Part II's page text was not legible in the scan we fetched.)
- **UREI Model 920-16 brochure / spec sheet.** Specs, applications, dimensions. https://www.barryrudolph.com/recall/manuals/urei_universal_audio_cooper_time_cube.pdf
- **UREI Model 920-16 Cooper Time Cube Instruction Manual (33-page scan; copy supplied by Patrick).** Contents: theory of operation, installation, operation, maintenance, the factory frequency-response chart (serial 117), schematic D11688 (11-21-71), the 1109 data sheet and schematic B11195C, the Production Calibration Procedure, and a handwritten "14 MS Line Specimen #2972" sketch. **Transcribed in doc 06.** The same scan is also hosted by Effectrode: https://www.effectrode.com/wp-content/uploads/2023/03/cooper_time_cube_urei_920_16_manual.pdf
- **Whyte, B., "Behind the Scenes," *Audio*, Jul 1972.** Covers the production design (polyethylene vs copper), the pre/post-EQ signal path, $850 price, and why the delays differ. https://gammaelectronics.xyz/audio_07-1972_behind-the-scene.html
- ***Audio*, Jan 1972, "Behind the Scenes."** Cooper tube background; compares a Japanese 58 ft tube unit with "honky" character. https://www.worldradiohistory.com/Archive-All-Audio/Archive-Audio/70s/Audio-1972-01.pdf
- **UREI ads, *Recording Engineer/Producer*, Jan/Feb and Mar/Apr 1972.** Applications; claimed 30 Hz–10 kHz bandwidth. https://www.worldradiohistory.com/Archive-All-Audio/Archive-Recording-Engineer/70s/Recording-1972-04.pdf

## Manufacturer and emulation material

- **Berners, D., "Wiggly Frequency Response and the Cooper Time Cube"** (UA). The key modeling explanation: echo at 2τ after the main pulse, low-passed, and cascade nulls. https://www.uaudio.fr/blog/wiggly-frequency-response-cooper-time-cube/
- **UA, "The Cooper Time Cube: Recreating a Cult Classic."** Internals (Shure SM57-type capsules, tooled aluminum blocks, springs, packing peanuts), 1109 amps, Mk II features. https://www.uaudio.com/blogs/ua/cooper-time-cube-power
- **UA Cooper Time Cube MkII manual.** Control ranges. https://help.uaudio.com/hc/en-us/articles/33030860246676
- **UA WebZine 2005, "UREI ObscUREIties."** About 1,000 sold; plate pre-delay use. https://uaudio.fr/webzine/2005/august/text/content4.html
- **AudioThing Echo Cube.** https://www.audiothing.net/effects/echo-cube/
- **Eventide, "A Brief History of Time Delay."** Historical context. https://www.eventideaudio.com/?p=93532
- **GroupDIY "Cooper time cube" thread.** Describes the 1109 as a class-AB op-amp with I/O transformers, and gives schematic page references. https://groupdiy.com/threads/cooper-time-cube.53256

## Field research (doc 07)

- **R/E/P, "Cooper Time Cube Problem" (2008).** John Noll's repair notes (SM57 capsule replacement, metal resonator plus O-ring, ~200 Ω driver); Brian Kehew on Ray Lubow and copper tube. https://repforums.prosoundweb.com/index.php?topic=23924.0
- **R/E/P, "UREi Cooper Time Cube Questions" (2006–2009).** The "Demo 99" early unit with food-grade hose; Fletcher's "400Hz-4kHz" recollection; the Xophonic; the St. Croix modeling recollection. https://repforums.prosoundweb.com/index.php?topic=14183.0
- **Gearspace, "d.i.y. cooper time cube."** Internal layout described: clear delay hose, green ¼-wave stub, SM57-type capsules, Tee block with wax-sealed screws. https://gearspace.com/threads/d-i-y-cooper-time-cube.779573/
- **Gearspace, "Cooper Time Cube Mic Replacement" (2008).** Pickup capsule ¾″ × 1⅛″. https://gearspace.com/threads/cooper-time-cube-mic-replacement.178486/
- **UA internal photo** used for the bore estimate: https://media.uaudio.com/assetlibrary/b/l/blog_cooper_time_cube_power_feat5_@2x.jpg
- **Shure AL377B, "Accessories and Replacement Parts for Microphones"** (R50 and R70 cross-reference). https://content-files.shure.com/KnowledgeBaseFiles/oem-cross-reference/al377b_accessories_and_replacement_parts_for_microphones.pdf
- **Shure 1966 catalog** (570/570S spec; R50 model list). https://www.worldradiohistory.com/Archive-Company-Publications/Shure/1966_shure.pdf
- **Shure Model 561 user guide** (R50 cartridge; typical frequency response figure). https://www.manualslib.com/manual/739022/Shure-561.html?page=2
- **samples.but.happy, "Cooper Time Cube Impulse Responses"** (A, B, and cascade AIFs from an original unit; commercial). https://www.samplesbuthappy.com/shop/p/cooper-time-cube-ir
- **Library of Congress copy of the 920-16 manual** (the scan Patrick supplied). https://tile.loc.gov/storage-services/master/mbrs/recording_preservation/manuals/UREI%20Univesal%20Audio%20Cooper%20Time%20Cube%20(Model%20920-16).pdf

## DSP and acoustics

- **Abel, J., Smyth, T., Smith, J. O., "A Simple, Accurate Wall Loss Filter for Acoustic Tubes," DAFx-03.** Minimum-phase loss filter; closed-form first-order shelf cascade. https://www.ece.uvic.ca/~bctill/papers/numacoust/Abel_etal_2003.pdf
- **Smith, J. O. & Rocchesso, D., "Lossy/Dispersive Waveguides."** Commuting losses to waveguide ends. https://ccrma.stanford.edu/~jos/wgj/Lossy_Dispersive_Waveguides.html
- **Smith, J. O., *Physical Audio Signal Processing*.** Delay plus filter model for propagation. https://ccrma.stanford.edu/~jos/pasp05/Summary.html
- **Smith, J. O., "Frequency-Dependent Friction."** The 3.0×10⁻⁵·√f / a tube-loss approximation. https://ccrma.stanford.edu/~jos/gdwn/Frequency_Dependent_Friction.html
- **Christensen et al., "Practical modeling of acoustic losses in air due to heat conduction and viscosity" (2008).** Full Bessel-function visco-thermal model. https://www.ece.uvic.ca/~bctill/papers/numacoust/Christensen_etal_2008.pdf
- **Laakso, Välimäki, Karjalainen, Laine, "Splitting the Unit Delay," IEEE SP Magazine 13(1), 1996.** Fractional delay: Lagrange, Thiran. doi:10.1109/79.482137
- **Välimäki & Laakso, "Principles of Fractional Delay Filters," ICASSP 2000.**
- **JUCE `dsp::DelayLine` interpolation types** (None, Linear, Lagrange3rd, Thiran). https://github.com/juce-framework/JUCE/blob/master/modules/juce_dsp/processors/juce_DelayLine.h
- **Abel, J. & Berners, D., "Discrete-Time Shelf Filter Design for Analog Modeling," AES 115th Conv., Paper 5939 (2003).** For pre/post-EQ shelving. https://aes.org/publications/elibrary-page/?id=12440

## Gaps and access notes

- Gearspace and R/E/P block automated fetchers. They were read later through a normal browser session (doc 07). The "400 Hz–5 kHz" figure in the original brief traces to forum recollections there and is contradicted by the factory documents.
- The samples.but.happy store is no longer operating (confirmed 2026-11-06); only its search-listing text is cited.
- The Effectrode manual scan could not be downloaded through this workspace's network proxy. Patrick supplied the same manual directly.
