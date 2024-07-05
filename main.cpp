//
// librealsense2 を使って RealSense のデプス画像とカラー画像を表示する
//

// RealSense
#include<librealsense2/rs.hpp>

// OpenCV
#include <opencv2/opencv.hpp>

// Windows (Visual Studio) 用の設定
#if defined(_MSC_VER)
#  // コンフィギュレーションを調べる
#  if defined(_DEBUG)
#    // デバッグビルド用ライブラリをリンクする
#    define CONFIGURATION_STR "d.lib"
#  else
#    // Visual Studio のリリースビルドではコンソールを出さない
#    pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#    // リリースビルド用ライブラリをリンクする
#    define CONFIGURATION_STR ".lib"
#  endif
#  // リンクする OpenCV のライブラリのバージョン
#  define CV_VERSION_STR CVAUX_STR(CV_MAJOR_VERSION) CVAUX_STR(CV_MINOR_VERSION) CVAUX_STR(CV_SUBMINOR_VERSION)
#  // リンクするライブラリを指定する
#  pragma comment(lib, "opencv_world" CV_VERSION_STR CONFIGURATION_STR)
#  pragma comment(lib, "realsense2.lib")
#endif

// 測定範囲のデプスの最大値 (単位 m)
constexpr auto MAX_DISTANCE{ 5.0 };

// 標準ライブラリ
#include <iostream>

int main(int argc, char* argv[]) try
{
	// パイプラインを生成する
	rs2::pipeline pipe;

	// デフォルト設定でパイプラインを起動して選択されたデバイス特性を取得する
	auto selection{ pipe.start() };

	// デバイス特性からデプスセンサを取り出す
	auto sensor{ selection.get_device().first<rs2::depth_sensor>() };

	// デプスセンサの値の単位をメートルに換算する係数を取得する
	const auto depth_scale{ sensor.get_depth_scale() };

	// デプス画像の位置をカラー画像に合わせるフィルタを作成する（これ遅い）
	rs2::align align{ RS2_STREAM_COLOR };

	// センサから取得するフレームセットの格納先
	rs2::frameset frames;

	// 1ms 待って ESC = 27 が押されたら終了する
	while (cv::waitKey(1) != 27)
	{
		// フレームセットを取得する
		if (pipe.poll_for_frames(&frames))
		{
			// フレームセットからカラー画像に合わせたデプス画像とカラー画像を取り出す
			auto aligned_frames{ align.process(frames) };

			// フレームセットからデプス画像とカラー画像をビデオフレームとして取り出す
			rs2::video_frame aligned_depth_frame{ aligned_frames.first(RS2_STREAM_DEPTH) };
			rs2::video_frame aligned_color_frame{ aligned_frames.first(RS2_STREAM_COLOR) };

			// デプス画像とカラー画像の幅と高さを取得する
			const auto depth_width{ aligned_depth_frame.get_width() };
			const auto depth_height{ aligned_depth_frame.get_height() };
			const auto color_width{ aligned_color_frame.get_width() };
			const auto color_height{ aligned_color_frame.get_height() };

			// デプス画像とカラー画像のデータのポインタを取得する
			const auto depth_ptr{ static_cast<const uint16_t*>(aligned_depth_frame.get_data()) };
			const auto color_ptr{ static_cast<const uchar*>(aligned_color_frame.get_data()) };

			// デプス画像とカラー画像のデータを OpenCV の配列に変換する
			const cv::Mat depth_data{ depth_height, depth_width, CV_16U, const_cast<uint16_t*>(depth_ptr) };
			const cv::Mat color_data{ color_height, color_width, CV_8UC3, const_cast<uchar*>(color_ptr) };

			// デプス画像とカラー画像のデータを表示できるように変換する
			cv::Mat depth, color;
			depth_data.convertTo(depth, CV_8U, 255.0 * depth_scale / MAX_DISTANCE);
			cv::cvtColor(color_data, color, cv::COLOR_RGB2BGR);

			// 画像を表示する
			cv::imshow("depth", depth);
			cv::imshow("color", color);
		}
	}

	return EXIT_SUCCESS;
}
catch (const rs2::error& e)
{
	// RealSense が投げた例外を表示する
	std::cerr
		<< "RealSense error calling "
		<< e.get_failed_function()
		<< "(" << e.get_failed_args() << "):\n    "
		<< e.what()
		<< std::endl;

	return EXIT_FAILURE;
}
catch (const std::exception& e)
{
	// その他の例外を表示する
	std::cerr
		<< e.what()
		<< std::endl;

	return EXIT_FAILURE;
}
