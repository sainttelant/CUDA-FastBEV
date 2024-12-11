/*
 * @Author: Mandy 
 * @Date: 2023-08-13 13:19:50 
 * @Last Modified by: Mandy
 * @Last Modified time: 2023-08-13 18:51:21
 */
#include <cuda_runtime.h>
#include <string.h>
#include <iostream>
#include <sstream>
#include <fstream>
#include <vector>

#define STB_IMAGE_IMPLEMENTATION
#include "stb_image.h"

#define STB_IMAGE_WRITE_IMPLEMENTATION
#include <stb_image_write.h>
#include <algorithm>

#include "fastbev/fastbev.hpp"
#include "common/check.hpp"
#include "common/tensor.hpp"
#include "common/timer.hpp"
#include "common/visualize.hpp"

const int final_height = 256;
const int final_weith = 704;

static std::vector<unsigned char*> load_images(const std::string& root) {
  const char* file_names[] = {"0-FRONT.jpg", "1-FRONT_RIGHT.jpg", "2-FRONT_LEFT.jpg",
                              "3-BACK.jpg",  "4-BACK_LEFT.jpg",   "5-BACK_RIGHT.jpg"};

  std::vector<unsigned char*> images;
  for (int i = 0; i < 6; ++i) {
    char path[200];
    sprintf(path, "%s/%s", root.c_str(), file_names[i]);

    int width, height, channels;
    images.push_back(stbi_load(path, &width, &height, &channels, 0));
  }
  return images;
}


static void free_images(std::vector<unsigned char*>& images) {
  for (size_t i = 0; i < images.size(); ++i) stbi_image_free(images[i]);

  images.clear();
}


std::shared_ptr<fastbev::Core> create_core(const std::string& model, const std::string& precision) {
  printf("Create by %s, %s\n", model.c_str(), precision.c_str());
  fastbev::pre::NormalizationParameter normalization;
  normalization.image_width = 1600;
  normalization.image_height = 900;
  normalization.output_width = 704;
  normalization.output_height = 256;
  normalization.num_camera = 6;
  normalization.resize_lim = 0.44f;
  normalization.interpolation = fastbev::pre::Interpolation::Nearest;

  float mean[3] = {123.675, 116.28, 103.53};
  float std[3] = {58.395, 57.12, 57.375};
  normalization.method = fastbev::pre::NormMethod::mean_std(mean, std, 1.0f, 0.0f);
  fastbev::pre::GeometryParameter geo_param;
  geo_param.feat_height = 64;
  geo_param.feat_width = 176;
  geo_param.num_camera =6;
  geo_param.valid_points = 160000;
  geo_param.volum_x = 200;
  geo_param.volum_y = 200;
  geo_param.volum_z = 4;

  fastbev::CoreParameter param;
  param.pre_model = nv::format("model/%s/build/fastbev_pre_trt.plan", model.c_str());
  param.normalize = normalization;
  param.post_model = nv::format("model/%s/build/fastbev_post_trt_decode.plan", model.c_str());
  param.geo_param = geo_param;
  return fastbev::create_core(param);
}

cv::Mat draw_boxes(std::vector<fastbev::post::transbbox::BoundingBox> bboxes)
{ 
  // 每米10个像素
  int bevsize_w = 800;
  int bevsize_h = 600;
  int show_range = 50;  // 显示范围  ,车位于图像中央 前后加起来100m

  float scale = (bevsize_h * 0.5) / show_range;   // 车位于图像中心,根据50m对应500个像素计算每个像素代表多少米

  // 创建一张白图
  int center_ego_u = bevsize_w / 2;
  int center_ego_v = bevsize_h / 2;
  
  // 创建一张size大小俯视图
  // cv::Mat img(bevsize_h, bevsize_w, CV_8UC3, cv::Scalar(255,255,255));
  cv::Mat img(bevsize_h, bevsize_w, CV_8UC3, cv::Scalar(0, 0, 0));
  // 车体绘制车中心
  cv::circle(img, cv::Point(center_ego_u, center_ego_v), 10, cv::Scalar(0, 0, 0), cv::FILLED);
  
  
  // 绘制模拟的雷达圈
  for(int i = 0; i < 16; i++)
  {
    int r_canvas = scale * i * 8;
    cv::circle(img, cv::Point(center_ego_u, center_ego_v), r_canvas, cv::Scalar(0, 0, 255), 1, cv::FILLED);
  }
       
  //                           up z    x front (yaw=0)
  //                              ^   ^
  //                              |  /
  //                              | /
  //  (yaw=0.5*pi) left y <------ 0     
  
  for(auto& obj : bboxes)
  {

    
    int u = (bevsize_w / 2) - obj.position.y * scale;
    int v = (bevsize_h / 2) - obj.position.x * scale;
    
    int w = obj.size.w * scale;
    int h = obj.size.l * scale;

    // (90 - θ)表示θ的余角(与图像x负方向的夹角),  这里 (90 - θ) % 180  表示 (90 - θ)的补角  + 360表示取正 
    int rot = int(90 - obj.z_rotation / M_PI * 180 + 360) % 180;
    // 创建一个旋转的矩形 旋转中心, 宽高 旋转角度(水平轴顺时针角度)
    cv::RotatedRect box(cv::Point(u, v), cv::Size(w, h), rot);
    
    cv::Point2f vertex[4];
    box.points(vertex);
    for (int i = 0; i < 4; i++)
      // vertex[(i + 1) % 4 循环连接下一个点
      cv::line(img, vertex[i], vertex[(i + 1) % 4], cv::Scalar(0, 255, 255), 1, cv::LINE_AA);
    
    // std::string text = label_map[obj.id];
    std::string text = std::to_string(obj.id);
    cv::putText(img, text, (cv::Point(u , v + h / 2 + 5)), 0, 0.5, cv::Scalar(255, 255, 255), 1, 16);
  }
    cv::rotate(img, img, cv::ROTATE_90_CLOCKWISE);

    cv::Point po(img.size().width /2, img.size().height /2);
    cv::Point px(img.size().width /2, img.size().height /2 - 50);
    cv::Point py(img.size().width /2 - 50, img.size().height /2);

    cv::line(img, po, px, cv::Scalar(0, 0, 255), 2, 8);
    cv::line(img, po, py, cv::Scalar(0, 255, 0), 2, 8);
    cv::circle(img,po, 4, cv::Scalar(255, 0, 0), -1, 8);


    return img;
}



void SaveBoxPred(std::vector<fastbev::post::transbbox::BoundingBox> boxes, std::string file_name)
{
    std::ofstream ofs;
    ofs.open(file_name, std::ios::out);
    if (ofs.is_open()) {
        for (const auto box : boxes) {
          ofs << box.position.x << " ";
          ofs << box.position.y << " ";
          ofs << box.position.z << " ";
          ofs << box.size.w << " ";
          ofs << box.size.l << " ";
          ofs << box.size.h << " ";
          ofs << box.z_rotation << " ";
          ofs << box.id << " ";
          ofs << box.score << " ";
          ofs << "\n";
        }
    }
    else {
      std::cerr << "Output file cannot be opened!" << std::endl;
    }
    ofs.close();
    std::cout << "Saved prediction in: " << file_name << std::endl;
    return;
};

int main(int argc, char** argv){


    const char* data = "example-data";
    const char* model = "resnet18";
    const char* precision = "fp16";

    if (argc > 1) data      = argv[1];
    if (argc > 2) model     = argv[2];
    if (argc > 3) precision = argv[3];

    std::string Save_Dir = nv::format("model/%s/result", model);
    auto core = create_core(model, precision);
    if (core == nullptr) {
      printf("Core has been failed.\n");
      return -1;
    }
 
    cudaStream_t stream;
    cudaStreamCreate(&stream);

    core->print();
    core->set_timer(true);

    auto images = load_images(data);

    auto valid_c_idx = nv::Tensor::load(nv::format("%s/valid_c_idx.tensor", data), false);
    auto valid_x = nv::Tensor::load(nv::format("%s/x.tensor", data), false);
    auto valid_y = nv::Tensor::load(nv::format("%s/y.tensor", data), false);
    core->update(valid_c_idx.ptr<float>(), valid_x.ptr<int64_t>(), valid_y.ptr<int64_t>(), stream);
    // warmup
    auto bboxes = core->forward((const unsigned char**)images.data(), stream);
    
    // evaluate inference time
    for (int i = 0; i < 5; ++i) {
      core->forward((const unsigned char**)images.data(), stream);
    }

    std::string save_file_name = Save_Dir + ".txt";
    SaveBoxPred(bboxes, save_file_name);

    cv::namedWindow("result", cv::WINDOW_NORMAL);
    cv::Mat results = draw_boxes(bboxes);

    cv::imshow("result bev", results);
    cv::imwrite("demo/result.jpg", results);
    cv::waitKey(0);
    std::cout << bboxes.size() << std::endl;
/*     for(auto &bbox : bboxes)
    { 
      // id x y z w l h yaw vx vy score
      std::cout << bbox.id << "  ";
      std::cout << bbox.position.x << "  " << bbox.position.y << "  " << bbox.position.z << "  ";
      std::cout << bbox.size.w << "  " << bbox.size.l << "  " << bbox.size.h << "  ";
      std::cout << bbox.z_rotation << "  ";
      std::cout << bbox.score << "  ";
      std::cout << bbox.velocity.vx << "  " << bbox.velocity.vy << "  ";
      std::cout << std::endl;
     } */

    // destroy memory
    free_images(images);
    cv::destroyAllWindows();
    checkRuntime(cudaStreamDestroy(stream));
    return 0;
}