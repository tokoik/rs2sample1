//
// librealsense2 ���g���� RealSense �̃f�v�X�摜�ƃJ���[�摜��\������
//

// RealSense
#include<librealsense2/rs.hpp>

// OpenCV
#include <opencv2/opencv.hpp>

// Windows (Visual Studio) �p�̐ݒ�
#if defined(_MSC_VER)
#  // �R���t�B�M�����[�V�����𒲂ׂ�
#  if defined(_DEBUG)
#    // �f�o�b�O�r���h�p���C�u�����������N����
#    define CONFIGURATION_STR "d.lib"
#  else
#    // Visual Studio �̃����[�X�r���h�ł̓R���\�[�����o���Ȃ�
#    pragma comment(linker, "/subsystem:\"windows\" /entry:\"mainCRTStartup\"")
#    // �����[�X�r���h�p���C�u�����������N����
#    define CONFIGURATION_STR ".lib"
#  endif
#  // �����N���� OpenCV �̃��C�u�����̃o�[�W����
#  define CV_VERSION_STR CVAUX_STR(CV_MAJOR_VERSION) CVAUX_STR(CV_MINOR_VERSION) CVAUX_STR(CV_SUBMINOR_VERSION)
#  // �����N���郉�C�u�������w�肷��
#  pragma comment(lib, "opencv_world" CV_VERSION_STR CONFIGURATION_STR)
#  pragma comment(lib, "realsense2.lib")
#endif

// ����͈͂̃f�v�X�̍ő�l (�P�� m)
constexpr auto MAX_DISTANCE{ 5.0 };

// �W�����C�u����
#include <iostream>
#include <algorithm>
#include <vector>
#include <cmath>

//
// �F��Ԃ𒆊Ԓl�ŕ�������
//
// ���� pixel �ɗ^����ꂽ�s�N�Z���̃��X�g��, �ł��L���͈͂����F��Ԃ̎���̒��Ԓl��, left �� right �ɕ�������.
//
void splitColorSpace(const std::vector<cv::Vec3b>& pixels, std::vector<cv::Vec3b>& left, std::vector<cv::Vec3b>& right)
{
  // �F��Ԃ̍ŏ��l�ƍő�l
  cv::Vec3b minPixel{ pixels[0] };
  cv::Vec3b maxPixel{ pixels[0] };

  // ���ׂĂ� pixel �ɂ���
  for (const auto& pixel : pixels)
  {
    // RGB ���ꂼ��̍ŏ��l�ƍő�l�����߂�
    for (int i = 0; i < 3; ++i)
    {
      minPixel[i] = std::min(minPixel[i], pixel[i]);
      maxPixel[i] = std::max(maxPixel[i], pixel[i]);
    }
  }

  // ���ƒl�͈̔͂�
  int axis{ 0 };
  int maxRange{ 0 };

  // RGB ���ꂼ��̎��ɂ���
  for (int i = 0; i < 3; ++i)
  {
    // �ő�l�ƍŏ��l�͈̔͂����߂�
    const int range{ maxPixel[i] - minPixel[i] };

    // �͈͂̍ő�l�Ƃ��̎������߂�
    if (range > maxRange)
    {
      maxRange = range;
      axis = i;
    }
  }

  // ���̒l�Ń\�[�g����
  std::vector<cv::Vec3b> sortedPixels{ pixels };
  std::sort(sortedPixels.begin(), sortedPixels.end(),
    [axis](const cv::Vec3b& a, const cv::Vec3b& b) { return a[axis] < b[axis]; }
  );

  // ���Ԓl�̃C���f�b�N�X�����߂�
  auto medianIndex{ static_cast<int>(sortedPixels.size()) / 2 };

  // ���Ԓl�� left �� right �ɕ�������
  left.assign(sortedPixels.begin(), sortedPixels.begin() + medianIndex);
  right.assign(sortedPixels.begin() + medianIndex, sortedPixels.end());
}

//
// �F��Ԃ��ċA�I�ɕ�������
//
// ���� pixels �ɗ^����ꂽ�s�N�Z���̃��X�g��, numColors �̐�������������.
//
std::vector<std::vector<cv::Vec3b>> recursiveSplit(const std::vector<cv::Vec3b>& pixels, int numColors)
{
  // 1 �F�Ȃ番�����Ȃ�
  if (numColors == 1) return { pixels };

  // �F��Ԃ𒆊Ԓl�� left �� right �ɕ�������
  std::vector<cv::Vec3b> left, right;
  splitColorSpace(pixels, left, right);

  // �������� left �� right �̂��ꂼ����ċA�I�ɕ�������
  std::vector<std::vector<cv::Vec3b>> result{ recursiveSplit(left, numColors / 2) };
  std::vector<std::vector<cv::Vec3b>> rightResult{ recursiveSplit(right, numColors / 2) };

  // left �� right ���Ăь�������
  result.insert(result.end(), rightResult.begin(), rightResult.end());

  // ���ʂ�Ԃ�
  return result;
}

//
// ����������Ԃ̑�\�F���v�Z����
//
// ���� partitions �ɗ^����ꂽ��Ԃ̃��X�g����, ���ꂼ��̑�\�F�����߂�.
//
std::vector<cv::Vec3b> getRepresentativeColors(const std::vector<std::vector<cv::Vec3b>>& partitions)
{
  // �F��u��������p���b�g
  std::vector<cv::Vec3b> palette;

  // ���ׂĂ̋�Ԃɂ���
  for (const auto& part : partitions)
  {
    // �F�̍��v
    cv::Vec3d sum{ 0, 0, 0 };

    // �e��Ԃ̂��ׂẲ�f�l�̍��v�����߂�
    for (const auto& pixel : part) sum += pixel;

    // ��f�l�̕��ς��\�F�Ƃ��ăp���b�g�Ɋi�[����
    palette.push_back(sum / static_cast<int>(part.size()));
  }

  // �p���b�g��Ԃ�
  return palette;
}

//
// ���̃s�N�Z�����p���b�g�̍ł��߂��F�ɒu��������
//
// ���� pixel �̊e��f�l����Ԃ̑�\�F palette �ɒu��������.
//
cv::Vec3b mapToPalette(const cv::Vec3b& pixel, const std::vector<cv::Vec3b>& palette)
{
  // �p���b�g�̍ł��߂��F
  cv::Vec3b bestColor{ palette[0] };

  // �p���b�g�̐F�Ƃ̍ŏ�����
  auto minDist{ INT_MAX };

  // �p���b�g�̂��ׂĂ̐F�ɂ���
  for (const auto& color : palette)
  {
    // ��f�n�ƃp���b�g�̐F�Ƃ̋���
    auto dist{ static_cast<int>(cv::norm(pixel - color)) };

    // �ŏ��������X�V����
    if (dist < minDist)
    {
      minDist = dist;
      bestColor = color;
    }
  }

  // �ł��߂��F��Ԃ�
  return bestColor;
}

//
// ���f�B�A���J�b�g�@�ɂ�錸�F
// 
// ���� image �̉摜�𒆊Ԓl�ŕ������� numColors �F�Ɍ��F����.
//
cv::Mat reduceColorsMedianCut(const cv::Mat& image, int numColors)
{
  // ��f�l���i�[����z��
  std::vector<cv::Vec3b> pixels;

  // �摜�̂��ׂẲ�f�l���ꎟ���̔z��Ɋi�[����
  for (int y = 0; y < image.rows; ++y)
  {
    for (int x = 0; x < image.cols; ++x)
    {
      pixels.push_back(image.at<cv::Vec3b>(y, x));
    }
  }

  // ��f�l�𒆊Ԓl�� numColors �̋�Ԃɕ�������
  auto partitions{ recursiveSplit(pixels, numColors) };

  // �e��Ԃ̑�\�F�����߂�
  auto palette{ getRepresentativeColors(partitions) };

  // ���F�����摜
  cv::Mat newImage{ image.size(), image.type() };

  // ���ׂẲ�f�l���p���b�g�̍ł��߂��F�ɒu��������
  for (int y = 0; y < image.rows; ++y)
  {
    for (int x = 0; x < image.cols; ++x)
    {
      newImage.at<cv::Vec3b>(y, x) = mapToPalette(image.at<cv::Vec3b>(y, x), palette);
    }
  }

  // ���F�����摜��Ԃ�
  return newImage;
}

//
// K-means�@�ɂ�錸�F
//
// ���� image �̉摜�𒆊Ԓl�ŕ������� k �F�Ɍ��F����.
//
cv::Mat reduceColorsKMeans(const cv::Mat& image, int k)
{
  // �摜���ꎟ���ɕϊ�����
  cv::Mat imageReshaped{ image.reshape(1, image.rows * image.cols) };

  // �ꎟ���ɕϊ������摜�� float �^�ɕϊ�����
  imageReshaped.convertTo(imageReshaped, CV_32F);

  // K-means�N���X�^�����O�̐ݒ�
  cv::Mat labels, centers;

  // K-means�N���X�^�����O�����s����
  cv::kmeans(imageReshaped, k, labels,
    cv::TermCriteria(cv::TermCriteria::EPS + cv::TermCriteria::MAX_ITER, 100, 0.2),
    10, cv::KMEANS_RANDOM_CENTERS, centers);

  // �N���X�^�̒��S�� uchar �ɕϊ�����
  centers.convertTo(centers, CV_8U);

  // �N���X�^�̃��x�����ꎟ���ɕϊ�����
  cv::Mat resultImage{ labels.reshape(1, image.rows) };

  // �ꎟ���ɕϊ��������x���� uchar �ɕϊ�����
  resultImage.convertTo(resultImage, CV_8U);

  // �o�͉摜
  cv::Mat finalImage(image.size(), image.type());

  // �e��f�̒l���N���X�^�̒��S�̐F�ɒu��������
  for (int i = 0; i < resultImage.rows; ++i)
  {
    for (int j = 0; j < resultImage.cols; ++j)
    {
      int clusterIdx = resultImage.at<uchar>(i, j);
      finalImage.at<cv::Vec3b>(i, j) = centers.at<cv::Vec3b>(clusterIdx);
    }
  }

  // �o�͉摜��Ԃ�
  return finalImage;
}

//
// ���C���֐�
//
int main(int argc, char* argv[]) try
{
	// �p�C�v���C���𐶐�����
	rs2::pipeline pipe;

	// �f�t�H���g�ݒ�Ńp�C�v���C�����N�����đI�����ꂽ�f�o�C�X�������擾����
	auto selection{ pipe.start() };

	// �f�o�C�X��������f�v�X�Z���T�����o��
	auto sensor{ selection.get_device().first<rs2::depth_sensor>() };

	// �f�v�X�Z���T�̒l�̒P�ʂ����[�g���Ɋ��Z����W�����擾����
	const auto depth_scale{ sensor.get_depth_scale() };

	// �f�v�X�摜�̈ʒu���J���[�摜�ɍ��킹��t�B���^���쐬����i����x���j
	rs2::align align{ RS2_STREAM_COLOR };

	// �Z���T����擾����t���[���Z�b�g�̊i�[��
	rs2::frameset frames;

	// 1ms �҂��� ESC = 27 �������ꂽ��I������
	while (cv::waitKey(1) != 27)
	{
		// �t���[���Z�b�g���擾����
		if (pipe.poll_for_frames(&frames))
		{
			// �t���[���Z�b�g����J���[�摜�ɍ��킹���f�v�X�摜�ƃJ���[�摜�����o��
			auto aligned_frames{ align.process(frames) };

			// �t���[���Z�b�g����f�v�X�摜�ƃJ���[�摜���r�f�I�t���[���Ƃ��Ď��o��
			rs2::video_frame aligned_depth_frame{ aligned_frames.first(RS2_STREAM_DEPTH) };
			rs2::video_frame aligned_color_frame{ aligned_frames.first(RS2_STREAM_COLOR) };

			// �f�v�X�摜�ƃJ���[�摜�̕��ƍ������擾����
			const auto depth_width{ aligned_depth_frame.get_width() };
			const auto depth_height{ aligned_depth_frame.get_height() };
			const auto color_width{ aligned_color_frame.get_width() };
			const auto color_height{ aligned_color_frame.get_height() };

			// �f�v�X�摜�ƃJ���[�摜�̃f�[�^�̃|�C���^���擾����
			const auto depth_ptr{ static_cast<const uint16_t*>(aligned_depth_frame.get_data()) };
			const auto color_ptr{ static_cast<const uchar*>(aligned_color_frame.get_data()) };

			// �f�v�X�摜�ƃJ���[�摜�̃f�[�^�� OpenCV �̔z��ɕϊ�����
			const cv::Mat depth_data{ depth_height, depth_width, CV_16U, const_cast<uint16_t*>(depth_ptr) };
			const cv::Mat color_data{ color_height, color_width, CV_8UC3, const_cast<uchar*>(color_ptr) };

			// �f�v�X�摜�ƃJ���[�摜�̃f�[�^��\���ł���悤�ɕϊ�����
			cv::Mat depth, color;
			depth_data.convertTo(depth, CV_8U, 255.0 * depth_scale / MAX_DISTANCE);
			cv::cvtColor(color_data, color, cv::COLOR_RGB2BGR);

			// �J���[�摜�̋��E�ȊO���ڂ���
			cv::Mat color_filtered;
			cv::bilateralFilter(color, color_filtered, 9, 100.0, 10.0);
			cv::bilateralFilter(color_filtered, color, 9, 100.0, 10.0);

      color = reduceColorsMedianCut(color, 16);
      //color = reduceColorsKMeans(color, 16);

			// �摜��\������
			cv::imshow("depth", depth);
			cv::imshow("color", color);
		}
	}

	return EXIT_SUCCESS;
}
catch (const rs2::error& e)
{
	// RealSense ����������O��\������
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
	// ���̑��̗�O��\������
	std::cerr
		<< e.what()
		<< std::endl;

	return EXIT_FAILURE;
}
