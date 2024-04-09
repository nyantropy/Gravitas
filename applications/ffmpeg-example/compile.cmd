@rem Run these commands on your console.Change the ffmpeg dir to your directory
@rem set FFMPEG=..\ffmpeg-master-latest-win64-gpl-shared
@rem set PATH=%PATH%;%FFMPEG%\bin

set INC1=%FFMPEG%\include
set LIB1=%FFMPEG%\lib

cl /c /EHsc /I %INC1% encode_video.cpp
cl /DEBUG %LIB1%\avcodec.lib %LIB1%\avutil.lib %LIB1%\avformat.lib encode_video.obj

@rem encode_video.exe v.ES mpeg4


cl /c /EHsc /I %INC1% decode_video.cpp
cl /DEBUG %LIB1%\avcodec.lib %LIB1%\avutil.lib %LIB1%\avformat.lib decode_video.obj


@rem decode_video.exe v.ES v

cl /c /EHsc /ZI /I %INC1% video_debugging.cpp
cl /c /EHsc /I %INC1% transcode.cpp
cl /DEBUG %LIB1%\avcodec.lib %LIB1%\avutil.lib %LIB1%\avformat.lib transcode.obj video_debugging.obj


@rem transcode.exe small_bunny_1080p_60fps.mp4 bunny
