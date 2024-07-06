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
#include <algorithm>
#include <vector>
#include <cmath>

//
// 色空間を中間値で分割する
//
// 引数 pixel に与えられたピクセルのリストを, 最も広い範囲を持つ色空間の軸上の中間値で, left と right に分割する.
//
void splitColorSpace(const std::vector<cv::Vec3b>& pixels, std::vector<cv::Vec3b>& left, std::vector<cv::Vec3b>& right)
{
  // 色空間の最小値と最大値
  cv::Vec3b minPixel{ pixels[0] };
  cv::Vec3b maxPixel{ pixels[0] };

  // すべての pixel について
  for (const auto& pixel : pixels)
  {
    // RGB それぞれの最小値と最大値を求める
    for (int i = 0; i < 3; ++i)
    {
      minPixel[i] = std::min(minPixel[i], pixel[i]);
      maxPixel[i] = std::max(maxPixel[i], pixel[i]);
    }
  }

  // 軸と値の範囲と
  int axis{ 0 };
  int maxRange{ 0 };

  // RGB それぞれの軸について
  for (int i = 0; i < 3; ++i)
  {
    // 最大値と最小値の範囲を求める
    const int range{ maxPixel[i] - minPixel[i] };

    // 範囲の最大値とその軸を求める
    if (range > maxRange)
    {
      maxRange = range;
      axis = i;
    }
  }

  // 軸の値でソートする
  std::vector<cv::Vec3b> sortedPixels{ pixels };
  std::sort(sortedPixels.begin(), sortedPixels.end(),
    [axis](const cv::Vec3b& a, const cv::Vec3b& b) { return a[axis] < b[axis]; }
  );

  // 中間値のインデックスを求める
  auto medianIndex{ static_cast<int>(sortedPixels.size()) / 2 };

  // 中間値で left と right に分割する
  left.assign(sortedPixels.begin(), sortedPixels.begin() + medianIndex);
  right.assign(sortedPixels.begin() + medianIndex, sortedPixels.end());
}

//
// 色空間を再帰的に分割する
//
// 引数 pixels に与えられたピクセルのリストを, numColors の数だけ分割する.
//
std::vector<std::vector<cv::Vec3b>> recursiveSplit(const std::vector<cv::Vec3b>& pixels, int numColors)
{
  // 1 色なら分割しない
  if (numColors == 1) return { pixels };

  // 色空間を中間値で left と right に分割する
  std::vector<cv::Vec3b> left, right;
  splitColorSpace(pixels, left, right);

  // 分割した left と right のそれぞれを再帰的に分割する
  std::vector<std::vector<cv::Vec3b>> result{ recursiveSplit(left, numColors / 2) };
  std::vector<std::vector<cv::Vec3b>> rightResult{ recursiveSplit(right, numColors / 2) };

  // left と right を再び結合する
  result.insert(result.end(), rightResult.begin(), rightResult.end());

  // 結果を返す
  return result;
}

//
// 分割した区間の代表色を計算する
//
// 引数 partitions に与えられた区間のリストから, それぞれの代表色を求める.
//
std::vector<cv::Vec3b> getRepresentativeColors(const std::vector<std::vector<cv::Vec3b>>& partitions)
{
  // 色を置き換えるパレット
  std::vector<cv::Vec3b> palette;

  // すべての区間について
  for (const auto& part : partitions)
  {
    // 色の合計
    cv::Vec3d sum{ 0, 0, 0 };

    // 各区間のすべての画素値の合計を求める
    for (const auto& pixel : part) sum += pixel;

    // 画素値の平均を代表色としてパレットに格納する
    palette.push_back(sum / static_cast<int>(part.size()));
  }

  // パレットを返す
  return palette;
}

//
// 元のピクセルをパレットの最も近い色に置き換える
//
// 引数 pixel の各画素値を区間の代表色 palette に置き換える.
//
cv::Vec3b mapToPalette(const cv::Vec3b& pixel, const std::vector<cv::Vec3b>& palette)
{
  // パレットの最も近い色
  cv::Vec3b bestColor{ palette[0] };

  // パレットの色との最小距離
  auto minDist{ INT_MAX };

  // パレットのすべての色について
  for (const auto& color : palette)
  {
    // 画素地とパレットの色との距離
    auto dist{ static_cast<int>(cv::norm(pixel - color)) };

    // 最小距離を更新する
    if (dist < minDist)
    {
      minDist = dist;
      bestColor = color;
    }
  }

  // 最も近い色を返す
  return bestColor;
}

//
// メディアンカット法による減色
// 
// 引数 image の画像を中間値で分割して numColors 色に減色する.
//
cv::Mat reduceColorsMedianCut(const cv::Mat& image, int numColors)
{
  // 画素値を格納する配列
  std::vector<cv::Vec3b> pixels;

  // 画像のすべての画素値を一次元の配列に格納する
  for (int y = 0; y < image.rows; ++y)
  {
    for (int x = 0; x < image.cols; ++x)
    {
      pixels.push_back(image.at<cv::Vec3b>(y, x));
    }
  }

  // 画素値を中間値で numColors 個の区間に分割する
  auto partitions{ recursiveSplit(pixels, numColors) };

  // 各区間の代表色を求める
  auto palette{ getRepresentativeColors(partitions) };

  // 減色した画像
  cv::Mat newImage{ image.size(), image.type() };

  // すべての画素値をパレットの最も近い色に置き換える
  for (int y = 0; y < image.rows; ++y)
  {
    for (int x = 0; x < image.cols; ++x)
    {
      newImage.at<cv::Vec3b>(y, x) = mapToPalette(image.at<cv::Vec3b>(y, x), palette);
    }
  }

  // 減色した画像を返す
  return newImage;
}

//
// K-means法による減色
//
// 引数 image の画像を中間値で分割して k 色に減色する.
//
cv::Mat reduceColorsKMeans(const cv::Mat& image, int k)
{
  // 画像を一次元に変換する
  cv::Mat imageReshaped{ image.reshape(1, image.rows * image.cols) };

  // 一次元に変換した画像を float 型に変換する
  imageReshaped.convertTo(imageReshaped, CV_32F);

  // K-meansクラスタリングの設定
  cv::Mat labels, centers;

  // K-meansクラスタリングを実行する
  cv::kmeans(imageReshaped, k, labels,
    cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 0.2),
    10, cv::KMEANS_RANDOM_CENTERS, centers);

  // クラスタの中心を uchar に変換する
  centers.convertTo(centers, CV_8U);

  // クラスタのラベルを一次元に変換する
  cv::Mat resultImage{ labels.reshape(1, image.rows) };

  // 一次元に変換したラベルを uchar に変換する
  resultImage.convertTo(resultImage, CV_8U);

  // 出力画像
  cv::Mat finalImage(image.size(), image.type());

  // 各画素の値をクラスタの中心の色に置き換える
  for (int i = 0; i < resultImage.rows; ++i)
  {
    for (int j = 0; j < resultImage.cols; ++j)
    {
      int clusterIdx = resultImage.at<uchar>(i, j);
      finalImage.at<cv::Vec3b>(i, j) = centers.at<cv::Vec3b>(clusterIdx);
    }
  }

  // 出力画像を返す
  return finalImage;
}

//
// メイン関数
//
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

			// カラー画像の境界以外をぼかす
			cv::Mat color_filtered;
			cv::bilateralFilter(color, color_filtered, 9, 100.0, 10.0);
			cv::bilateralFilter(color_filtered, color, 9, 100.0, 10.0);

      color = reduceColorsMedianCut(color, 16);
      //color = reduceColorsKMeans(color, 16);

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
