# newzFM

newzFM是一个基于苹果IOS SDK里的音频类子项目iPhoneMixerEQGraphTest的二次开发，我只是在解码音频时加入pthread控制好音频流的下载和解码之间的缓冲，在播放时加入一个
环结构做播放队列的缓冲。。其中解码部分使用ISO标准里提供的mp3解码器代码（但我进行了部分内容的修改，并按照自己的理解加上了部分注释，实现了流媒体的seek功能等， huffman
解码等使用的是静态二叉树的硬解码技术，很多环节还有优化的空间），

DONE:
1.实时播放网络mp3音频流
2.加入Graph Unit均衡器

ps：暂时已经知道的一些bugs：
1.偶尔会出现Buffer overflow的错误；（不是必现）
2.seek播放时，偶尔会出现杂音的错误；（不是必现）
3.seek到最后，然后播放下一首时，会出现一点杂音的错误（必现）

psps：时间仓促，加上网络环境，可能会存在一些未知道的bugs

Limit:
1.只支持mp3格式的音频，即MPEG-1，LAYER 3的格式


TODO:
1.在弱网时，加入指数规避算法，控制好下载缓存
2.断点续传，多点下载
3.录音，多音轨混音，实时转码
4.混响器
5.MIDI？
6.人声消除？
7.网络电话？
