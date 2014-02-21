SplotsVideoRecorder
===================
A common problem for android developers is a stop motion video recording like vine, instagram etc in mp4.
Currently the only way for encoding in H264 is with ffmpeg or the android mediacodec, but this solutions
have disadvanteges like overweight, not backward compatible to older API levels or the commercial use comes 
with license fees to the MPEG LA. Since cisco is developing the openh264 project, there is a solution to
these problems. 

- Audio encoding in AAC with the Fraunhofer Codec
- Video encoding with the openh264 (wels) library
  Project homepage: http://www.openh264.org
  Git repo: https://github.com/cisco/openh264
- Muxing audio and video to mp4 file with Mp4Parser
  Source: https://code.google.com/p/mp4parser
  
The goal is to combine these project to a simple to use library.
